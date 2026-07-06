#include "terminal/Processor.h"

namespace terminal
{
/*____________________________________________________________________________*/

Processor::Processor (Model& terminalModel)
    : model (terminalModel)
    , video (jam::Cell::Rectangle (jam::Cell (defaultCols), jam::Cell (defaultRows)), videoEvents, model)
    , parser (video)
    , cellFifo (placeholderRingCapacity, placeholderRingCapacity)
{
    videoEvents.writeToHost = { &onWriteToHost, this };
    videoEvents.pushLine    = { &onPushLine, this };

    model.addListener (this);
}

Processor::~Processor()
{
    model.removeListener (this);
    stop();
}

void Processor::prepare() noexcept
{
    auto* scrollbackLinesParameter {
        model.getParameter<jam::Parameter<int>> (model.getType(), ID::scrollbackLines)
    };
    jassert (scrollbackLinesParameter != nullptr);

    const int scrollbackLines { scrollbackLinesParameter->getValue() };
    const int historyCapacity { scrollbackLines * ringMaxCols + scrollbackLines };
    const int activeCapacity  { ringMaxCols * ringMaxRows };
    const int popbackCapacity { ringMaxCols * ringMaxRows };

    cellFifo.setSize (historyCapacity, activeCapacity, popbackCapacity);
}

void Processor::start (const juce::String& shell,
                       const juce::String& args,
                       const juce::String& workingDirectory)
{
    auto* winsizeParameter { model.getParameter<jam::Parameter<int>> (model.getType(), ID::winsize) };
    jassert (winsizeParameter != nullptr);

    const jam::Size<int16_t> winsize { winsizeParameter->getValue() };
    const auto [cols, rows] { winsize };
    const jam::Cell::Rectangle dims { jam::Cell (cols), jam::Cell (rows) };

    video.setWinsize (dims);

    registerTtyEvents();

#if JUCE_MAC || JUCE_LINUX
    tty = std::make_unique<jam::terminal::UnixTTY> (ttyEvents);
#elif JUCE_WINDOWS
    conptyDir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("END").getChildFile ("conpty");
    conptyDir.createDirectory();
    conptyDir.getChildFile ("conpty.dll")
        .replaceWithData (BinaryData::conpty_dll, static_cast<size_t> (BinaryData::conpty_dllSize));
    conptyDir.getChildFile ("OpenConsole.exe")
        .replaceWithData (BinaryData::OpenConsole_exe, static_cast<size_t> (BinaryData::OpenConsole_exeSize));

    tty = std::make_unique<jam::terminal::WindowsTTY> (ttyEvents, conptyDir);
#endif

    tty->open (dims, shell, args, workingDirectory);
}

void Processor::stop() noexcept
{
    if (tty != nullptr)
        tty->close();
}

void Processor::writeInput (const void* data, int numBytes) noexcept
{
    if (tty != nullptr)
        tty->write (static_cast<const char*> (data), numBytes);
}

bool Processor::drainHistory (jam::TextLine& outLine) noexcept
{
    return cellFifo.drainHistory (outLine);
}

bool Processor::drainActive (jam::TextLine& outLine) noexcept
{
    return cellFifo.drainActive (outLine);
}

void Processor::suspendProcessing (bool shouldBeSuspended) noexcept
{
    suspended.store (shouldBeSuspended, std::memory_order_release);
}

bool Processor::isSuspended() const noexcept
{
    return suspended.load (std::memory_order_acquire);
}

int Processor::getVisibleRows() const noexcept
{
    return video.getVisibleRows().value;
}

void Processor::pushPopback (const jam::Char* chars, int count, uint8_t flags) noexcept
{
    cellFifo.pushPopback (chars, count, flags);
}

void Processor::applyResize (jam::Cell::Rectangle dims) noexcept
{
    video.setWinsize (dims);

    if (tty != nullptr)
        tty->setWinsize (jam::terminal::Winsize { dims.getWidth().value, dims.getHeight().value, 0, 0 });
}

void Processor::parameterChanged (const juce::Identifier& id, const juce::var& newValue)
{
    // Wake seam only — touches no reader-owned state. Session owns the
    // live-resize orchestration (Resizer start/stop triggers, terminal/
    // Session.h) — this listener's job is done once the ValueTree write
    // that fired it has already been observed by that mechanism.
    juce::ignoreUnused (id, newValue);
}

void Processor::registerTtyEvents()
{
    ttyEvents.add<const char*, int> (jam::ID::data,
                                     [this] (const char* data, int length)
                                     {
                                         onData (data, length);
                                     });

    ttyEvents.add (jam::ID::drainComplete,
                   [this]
                   {
                       onDrainComplete();
                   });

    ttyEvents.add (jam::ID::shellExited,
                   [this]
                   {
                       onShellExited();
                   });
}

void Processor::onData (const char* data, int length) noexcept
{
    if (not isSuspended())
        parser.process (reinterpret_cast<const uint8_t*> (data), static_cast<size_t> (length));
}

void Processor::onDrainComplete() noexcept
{
    if (not isSuspended())
    {
        video.flushResponses();
        video.clearSyncOutputIfExpired();

        const int screen { video.getActiveScreen() };
        const auto& block { video.getBlock (screen) };
        const int rows { video.getVisibleRows().value };

        for (int row { 0 }; row < rows; ++row)
        {
            const auto* rowPointer { block.getRowPointer (row) };
            cellFifo.pushActive (rowPointer->chars, rowPointer->usedCols, toCellFifoFlags (rowPointer->flags));
        }
    }
}

void Processor::onShellExited() noexcept
{
    auto* shellExitedParameter {
        model.getParameter<jam::Parameter<int>> (jam::IDtype::session, jam::ID::shellExited)
    };
    jassert (shellExitedParameter != nullptr);

    shellExitedParameter->setValue (1);
}

void Processor::onWriteToHost (void* context, const char* data, int length) noexcept
{
    static_cast<Processor*> (context)->writeInput (data, length);
}

void Processor::onPushLine (void* context, int screen, const jam::Char* chars, int count) noexcept
{
    if (screen == jam::terminal::Screen::normal)
    {
        auto* self { static_cast<Processor*> (context) };
        self->cellFifo.pushHistory (chars, count, 0);
    }
}

uint8_t Processor::toCellFifoFlags (uint8_t rowFlags) noexcept
{
    const bool isContinued { (rowFlags & jam::Row::flexWrap) != 0 };
    const bool isJustified { (rowFlags & jam::Row::justify) != 0 };
    const uint8_t mark { static_cast<uint8_t> ((rowFlags & jam::Row::markMask) >> jam::Row::markShift) };

    return static_cast<uint8_t> ((isContinued ? jam::terminal::CellFifo::isContinuedFlag : 0)
                                | (isJustified ? jam::terminal::CellFifo::isJustifiedFlag : 0)
                                | (mark << jam::terminal::CellFifo::markShift));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
