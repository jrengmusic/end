/**
 * @file terminal/View.h
 * @brief Terminal view — PluginEditor analog. Holds Session reference.
 */
#pragma once
#include <JuceHeader.h>
#include "end/PaneView.h"
#include "terminal/Session.h"
#include "config/Config.h"
#include "Bimap.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal pane view — PluginEditor analog.
 *
 *  PaneView subclass. Holds Session reference (owned by Nexus). Parents
 *  (does not own) a jam::CodeView constructed over session.getDocument() —
 *  Session outlives View — Nexus destroyed after window.
 *
 *  Width SSOT (two-axis contract): resized() is the SOLE winsize author
 *  (AXIS 1) — scrollbar visibility never re-enters it. Font/zoom changes
 *  (AXIS 2, applyFont()) recompute cell metrics then re-enter AXIS 1 via
 *  resized() at applyFont()'s own tail — never write winsize directly.
 *  Padding and gutter (reserved scrollbar width) are still read ONCE from
 *  config::Model at construction time (ctor-time read only) — hot-reload
 *  wiring for those two is Phase 4 (ARCHITECTURE.md:507).
 *
 *  code.ligatures and the cursor block (style/blink/blink_interval; char/
 *  force plumbed only) ARE hot-reload wired — ValueTree::Listener on
 *  config::Model, single-key event dispatch through @c events, mirroring
 *  end::View/end::LookAndFeel's established pattern. code.font_family/
 *  font_size are hot-reload wired the same way, both routing to
 *  applyFont(). code.embolden is owned by end::LookAndFeel (the font owner,
 *  applyFontRasterization() precedent), not here.
 *
 *  Zoom (terminal::Model's own ID::zoom, Direction B) is hot-reload wired
 *  the same way as code.ligatures/font_family/font_size — a second
 *  ValueTree::Listener attachment, this time on session.getModel() (Direction
 *  A, Model.h's own doc comment: "terminal::View reacts via tree listeners"),
 *  registered in the events map (EventRegistration.cpp) and dispatched
 *  through the SAME valueTreePropertyChanged() below — also routes to
 *  applyFont(). Written by end::View's zoomIn/zoomOut/zoomReset actions
 *  (ActionRegistration.cpp), never by this class.
 *
 *  Phase 4: mouse events.
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
     *  end::LookAndFeel's established pattern). Fires for BOTH listened
     *  trees — config (config.addListener(this)) and session.getModel()
     *  (session.getModel().addListener(this)) — the events map key is a
     *  property/type Identifier regardless of which tree it arrived from. */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Encodes the key press via jam::Keyboard::map (applicationCursor
     *  + keyboardFlags read from session.getModel()'s NORMAL/ALTERNATE
     *  screen params at encode time, lock-free) and forwards the resulting
     *  byte sequence straight to session.writeInput() — keystrokes bypass
     *  the Model entirely; CodeModel is mutated ONLY by Session::drain().
     *  @return true when the key produced a non-empty sequence (consumed);
     *          false otherwise (JUCE falls through to any other handler). */
    bool keyPressed (const juce::KeyPress& key) override;

    // Phase 4: mouse events (mouse modes 1000/1002/1003/1006/1004 — no
    // jam_terminal OOTB encoder exists; flagged, not built this pass)

private:
    Session& session;

    std::unique_ptr<jam::CodeView> codeView;

    /** @brief Space between the pane edge and codeView, CSS order
     *  (top, right, bottom, left) — theme.lua code.padding. */
    juce::BorderSize<int> textPadding;

    /** @brief Event dispatch map — keyed by juce::Identifier (property or tree
     *  type). Populated by registerEvents(). Handles code.ligatures,
     *  code.font_family/font_size, terminal::Model's own ID::zoom (property
     *  keys), and the cursor block (IDtype::cursor tree-type key — style/
     *  blink/blink_interval re-applied together, char/force re-read and
     *  stored). Dispatched via single-key lookup in
     *  valueTreePropertyChanged() regardless of which listened tree
     *  (config or session.getModel()) the property arrived from.
     */
    jam::Function::Map<juce::Identifier, void> events;

    /** @brief Populates the events map with ValueTree property/type-keyed
     *  callbacks. Registers handlers for ID::ligatures (codeView->
     *  setLigatures()), ID::fontFamily/ID::fontSize/ID::zoom (applyFont() —
     *  ID::zoom arrives from session.getModel(), the other two from config),
     *  and jam::IDtype::cursor (applyCursorConfig() — style/blink/
     *  blink_interval/char/force re-applied together on any property
     *  change within the cursor block, mirroring end::LookAndFeel's
     *  per-tree-type colour refresh precedent). Defined in
     *  EventRegistration.cpp.
     */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
