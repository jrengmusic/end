#include "terminal/View.h"

namespace terminal
{
/*____________________________________________________________________________*/

View::View (jam::UUID uuid, jam::Model& appModel, Session& sessionRef)
    : end::PaneView (uuid, appModel)
    , session (sessionRef)
{
    codeView = std::make_unique<jam::CodeView> (session.getDocument());
    addAndMakeVisible (*codeView);

    // Additive observation, never interception — jam::CodeView's own
    // ContentView still handles local text selection natively through
    // JUCE's normal dispatch; this registration only lets mouse also see
    // the same events for xterm mouse-reporting purposes (terminal::Mouse's
    // own doc comment).
    codeView->addMouseListener (&mouse, false);

    // Applies the initial style state (cell metrics, cursor, ligatures) and
    // tail-calls resized() once — codeView is constructed first, per this
    // class's own doc comment.
    lookAndFeelChanged();

    model.addListener (this);
    registerEvents();
}

View::~View() { model.removeListener (this); }

void View::lookAndFeelChanged()
{
    const float zoom { model.getValue (IDtype::terminal, ID::zoom) };
    const auto metrics { lookAndFeel.getCodeMetrics (zoom) };

    codeView->setFont (metrics.font);
    codeView->setCellSize (metrics.cellWidth, metrics.cellHeight);

    // FT baseline wins over setFont()'s juce-ascent default — must run AFTER
    // setFont()/setCellSize() so this call is the one that sticks.
    codeView->setBaseline (metrics.baseline);

    mouse.setCellSize (metrics.cellWidth, metrics.cellHeight);

    // Direction B — cellSize published on every cell-metrics recompute.
    model.setCellSize (jam::Size<int16_t> (metrics.cellWidth, metrics.cellHeight));

    const auto cursorStyle { lookAndFeel.getCursorStyle() };

    if (end::CursorShape::getInstance()->contains (cursorStyle.style))
        codeView->setCaretShape (static_cast<jam::CaretShape> (end::CursorShape::get (cursorStyle.style)));

    codeView->setCaretBlink (cursorStyle.blink);
    codeView->setCaretBlinkInterval (cursorStyle.blinkInterval);

    codeView->setLigatures (lookAndFeel.getCodeLigatures());

    // AXIS 2 re-enters AXIS 1 — new cell metrics mean a new winsize.
    resized();
}

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

void View::resized()
{
    const auto content { lookAndFeel.getCodePadding().subtractedFrom (getLocalBounds()) };
    const int gutterWidth { lookAndFeel.getGutterWidth() };

    // Direction B cellSize — read LIVE, never cached (lookAndFeelChanged()
    // is the sole writer, this class's own doc comment).
    const jam::Size<int16_t> cellSize { static_cast<int> (model.getValue (IDtype::terminal, ID::cellSize)) };
    const auto [cellWidth, cellHeight] = cellSize;

    const int cols { (content.getWidth() - gutterWidth) / cellWidth };
    const int rows { content.getHeight() / cellHeight };

    codeView->setBounds (content);
    codeView->setViewportWidth (cols);
    codeView->setViewportLineCount (rows);

    model.setWinsize (jam::Size<int16_t> (cols, rows));

    // Unguarded — Session::start() is idempotent (Session.h), gated on its
    // own already-published winsize atom becoming positive.
    session.start();
}

bool View::keyPressed (const juce::KeyPress& key)
{
    return input.sendKeyPress (key);
}

void View::focusGained (FocusChangeType cause)
{
    end::PaneView::focusGained (cause);

    const bool focusEvents { model.getValue (jam::IDtype::modes, jam::ID::focusEvents) };

    if (focusEvents)
        session.writeInput (jam::terminal::Sequence::focusIn,
                            static_cast<int> (sizeof (jam::terminal::Sequence::focusIn) - 1));
}

void View::focusLost (FocusChangeType cause)
{
    end::PaneView::focusLost (cause);

    const bool focusEvents { model.getValue (jam::IDtype::modes, jam::ID::focusEvents) };

    if (focusEvents)
        session.writeInput (jam::terminal::Sequence::focusOut,
                            static_cast<int> (sizeof (jam::terminal::Sequence::focusOut) - 1));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
