#pragma once
#include <JuceHeader.h>
#include "PaneView.h"
#include "../Session.h"
#include "../Input.h"
#include "../Mouse.h"
#include "../LinkManager.h"

namespace terminal
{
/*____________________________________________________________________________*/

class Display
    : public PaneView
    , public jam::ValueTree::ComponentWithID<Display>
    , public juce::KeyListener
    , public juce::ValueTree::Listener
{
public:
    enum ColourIds
    {
        cursorColourId          = 0x3000003,
        selectionColourId       = 0x3000004,
        selectionCursorColourId = 0x3000005,
        hintLabelFgColourId     = 0x3000006,
        hintLabelBgColourId     = 0x3000007,
        ansi0ColourId           = 0x3000010,
        ansi1ColourId           = 0x3000011,
        ansi2ColourId           = 0x3000012,
        ansi3ColourId           = 0x3000013,
        ansi4ColourId           = 0x3000014,
        ansi5ColourId           = 0x3000015,
        ansi6ColourId           = 0x3000016,
        ansi7ColourId           = 0x3000017,
        ansi8ColourId           = 0x3000018,
        ansi9ColourId           = 0x3000019,
        ansi10ColourId          = 0x300001A,
        ansi11ColourId          = 0x300001B,
        ansi12ColourId          = 0x300001C,
        ansi13ColourId          = 0x300001D,
        ansi14ColourId          = 0x300001E,
        ansi15ColourId          = 0x300001F,
    };

    /**
     * @brief Constructs Display and parents Session's CodeView for rendering.
     *
     * Takes Session& — Display parents CodeView (owned by Session) for rendering
     * via addAndMakeVisible. Resize flows through State: CodeView writes viewport,
     * Session's Resizer calls prepare().
     *
     * @param session  The terminal session — provides CodeView (for rendering),
     *                 CodeModel (for document mutation), and Processor (for input/events).
     *                 Must outlive this Display.
     * @note MESSAGE THREAD.
     */
    Display (terminal::Session& session);

    ~Display() override;

    // PaneView interface
    juce::String getPaneType() const noexcept override;
    void applyZoom (float zoom) noexcept override;
    void enterSelectionMode() noexcept override;
    void copySelection() noexcept override;
    bool hasSelection() const noexcept override;

    // Deferred stubs
    bool isInSelectionMode() const noexcept;
    void exitSelectionMode() noexcept;
    void enterOpenFileMode() noexcept;
    void pasteClipboard();
    void writeToPty (const char* data, int len) noexcept;
    int getHintPage() const noexcept;
    int getHintTotalPages() const noexcept;

    /** @brief Returns a mutable reference to the Processor that backs this Display.
     *  @note MESSAGE THREAD. */
    terminal::Processor& getProcessor() noexcept { return processor; }

    // juce::Component
    void resized() override;
    void focusGained (FocusChangeType cause) override;

    // juce::KeyListener
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // juce::Component mouse events
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;

private:
    terminal::Session& session;
    terminal::Processor& processor;
    terminal::Model& state;

    jam::ValueTree::Attachment attachment;

    /** @brief Per-screen count of document tail lines Display laid down as the live region last tick.
     *  Internal transient — message thread only, never exposed. The live tail is dynamic
     *  (grows/shrinks per tick); indexed by screen so a screen switch never reads the wrong
     *  screen's extent. */
    std::array<int, Map::Screen::count> liveTailExtent {};

    terminal::LinkManager linkManager;
    terminal::Input input;
    terminal::Mouse mouse;

    /** @brief Dedicated listener for AppModel config changes (font, cursor, padding).
     *  Separated from Display's own ValueTree::Listener to avoid double-fire:
     *  SESSION is grafted under AppModel, so a single Listener on both trees
     *  receives terminal PARAM changes twice. */
    struct ConfigListener : private juce::ValueTree::Listener
    {
        explicit ConfigListener (Display& d) noexcept : display (d) {}
        void start() noexcept;
        void stop() noexcept;
    private:
        Display& display;
        juce::ValueTree appState;
        void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;
    };
    ConfigListener configListener { *this };

    void applyFromAppModel() noexcept;
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Display)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
