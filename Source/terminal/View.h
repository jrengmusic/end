/**
 * @file terminal/View.h
 * @brief Terminal view — PluginEditor analog. Holds Session reference.
 */
#pragma once
#include <JuceHeader.h>
#include "end/PaneView.h"
#include "terminal/Session.h"
#include "config/Config.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal pane view — PluginEditor analog.
 *
 *  PaneView subclass. Holds Session reference (owned by Nexus). Parents
 *  (does not own) a jam::CodeView constructed over session.getDocument() —
 *  Session outlives View — Nexus destroyed after window.
 *
 *  Width SSOT + reserved gutter: resized() is THE only width
 *  computation — scrollbar visibility never re-enters it. Cell metrics,
 *  padding, and gutter (reserved scrollbar width) are read ONCE from
 *  config::Model at construction time (ctor-time read only) — hot-reload
 *  listener wiring is Phase 4 (ARCHITECTURE.md:507).
 *
 *  code.ligatures and the cursor block (style/blink/blink_interval; char/
 *  force plumbed only) ARE hot-reload wired — ValueTree::Listener on
 *  config::Model, single-key event dispatch
 *  through @c events, mirroring end::View/end::LookAndFeel's established
 *  pattern. code.embolden is owned by end::LookAndFeel (the font owner,
 *  applyFontRasterization() precedent), not here.
 *
 *  Phase 4: ValueTree::Listener on terminal::Model, drain(), KeyListener,
 *           mouse events.
 */
class View
    : public end::PaneView
    , public juce::ValueTree::Listener
{
public:
    View (jam::UUID uuid, jam::Model& model, Session& sessionRef);

    ~View() override;

    Session& getSession() noexcept { return session; }

    /** @brief Width SSOT — computes cols/rows from component size,
     *  the reserved gutter, and the config-derived cell metrics; tells
     *  codeView the projection width and live-region row count. Scrollbar
     *  visibility never re-enters this computation (reserved gutter). */
    void resized() override;

    /** @brief Single-key dispatch through the events map — property key takes
     *  priority; falls back to @p tree.getType() (mirrors end::View/
     *  end::LookAndFeel's established pattern). */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    // Phase 4: focusGained, keyPressed, mouse events
    // Phase 4: terminal::Model listener, drain

private:
    config::Model& config { *config::Model::getInstance() };

    Session& session;

    /** @brief Parented and owned by this view, but never owns the document —
     *  the pure view over session.getDocument(). */
    std::unique_ptr<jam::CodeView> codeView;

    /** @brief Space between the pane edge and codeView, CSS order
     *  (top, right, bottom, left) — theme.lua code.padding. */
    juce::BorderSize<int> textPadding;

    /** @brief Reserved scrollbar gutter width in pixels — always
     *  subtracted from available width; scrollbar visibility toggles
     *  drawing only, never this reservation. */
    int gutterWidth { 0 };

    /** @brief Cell metrics in pixels, computed once at construction from
     *  config::Model — the single authority told to codeView
     *  via setCellSize(). */
    int cellWidthPx  { 0 };
    int cellHeightPx { 0 };

    /** @brief Cursor glyph character (theme.lua cursor.char) — PLUMB only.
     *  Glyph-char caret rendering is flagged, not built (the emoji/glyph-caret
     *  bundle); stored here for the
     *  DECSCUSR gate to consume. */
    juce::String cursorChar;

    /** @brief Cursor lock (theme.lua cursor.force) — PLUMB only. Stored here
     *  for the DECSCUSR gate: when true, programs cannot override
     *  cursorShape/cursorChar via DECSCUSR. */
    bool cursorForce { false };

    /** @brief Event dispatch map — keyed by juce::Identifier (property or tree
     *  type). Populated by registerEvents(). Handles code.ligatures (property
     *  key) and the cursor block (IDtype::cursor tree-type key — style/blink/
     *  blink_interval re-applied together, char/force re-read and stored).
     *  Dispatched via single-key lookup in valueTreePropertyChanged().
     */
    jam::Function::Map<juce::Identifier, void> events;

    /** @brief Populates the events map with ValueTree property/type-keyed
     *  callbacks. Registers handlers for ID::ligatures (codeView->
     *  setLigatures()) and jam::IDtype::cursor (applyCursorConfig() — style/
     *  blink/blink_interval/char/force re-applied together on any property
     *  change within the cursor block, mirroring end::LookAndFeel's
     *  per-tree-type colour refresh precedent). Defined in
     *  EventRegistration.cpp.
     */
    void registerEvents();

    /** @brief Reads the cursor block (style/blink/blink_interval/char/force)
     *  from config and applies style/blink/blink_interval to codeView; char/
     *  force are stored only (DECSCUSR gate). Called once at
     *  construction and again by the jam::IDtype::cursor event handler on
     *  hot-reload. Defined in EventRegistration.cpp.
     */
    void applyCursorConfig();

    /** @brief Seeds session.getDocument() with content exercising the
     *  validation gate (text, wrap, scroll, caret, styled runs, wide/emoji
     *  graphemes). HARNESS — remove. */
    void seedHarnessContent();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
