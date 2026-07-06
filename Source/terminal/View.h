/**
 * @file terminal/View.h
 * @brief Terminal view — PluginEditor analog. Holds Session reference.
 */
#pragma once
#include <JuceHeader.h>
#include "end/PaneView.h"
#include "terminal/Session.h"
#include "terminal/Input.h"
#include "terminal/Mouse.h"
#include "lookAndFeel/LookAndFeel.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal pane view — PluginEditor analog.
 *
 *  PaneView subclass. Holds Session reference (owned by Nexus). Parents
 *  (does not own) a jam::CodeView constructed over session.getDocument() —
 *  Session outlives View — Nexus destroyed after window.
 *
 *  @par Stateless rebuild — zero cached visual state
 *  Every visual value (font, cell size, padding, gutter) is read LIVE from
 *  its own source of truth on every use — @ref lookAndFeel for style/metrics,
 *  @ref model's own Direction B @c cellSize atom for the pixel cell size
 *  @ref resized() projects cols/rows from. This class caches NONE of it:
 *  no textPadding/gutterWidth/cellSize members survive between calls (unlike
 *  the prior revision) — @ref lookAndFeelChanged() computes metrics once via
 *  @ref lookAndFeel's getCodeMetrics(), publishes them (codeView, @ref mouse,
 *  @ref model), then tail-calls @ref resized(), which re-reads padding/
 *  gutter/cellSize live rather than trusting anything held across the call.
 *
 *  @par LookAndFeel pull (ARCHITECT ruling) — all visual style, fonts,
 *  metrics, AND ligatures are wired through @c end::LookAndFeel, never read
 *  directly from config here — this class carries no @c config::Model
 *  member at all. Every theme-driven value refreshes from
 *  @ref lookAndFeelChanged() — the single entry point JUCE invokes on theme
 *  reload (end::View's own ID::theme handler cascades
 *  @c sendLookAndFeelChange() down the whole component tree) — which calls
 *  @ref lookAndFeel's getCodeMetrics()/getCursorStyle()/getCodeLigatures(),
 *  applies each to @ref codeView, @ref mouse, and @ref model, then tail-calls
 *  @ref resized(). The constructor calls @ref lookAndFeelChanged() once,
 *  after codeView is constructed, to apply the initial style state.
 *
 *  @par Width SSOT (two-axis contract)
 *  resized() is the SOLE winsize author (AXIS 1) — scrollbar visibility
 *  never re-enters it. Font/zoom changes (AXIS 2, inside
 *  lookAndFeelChanged()) recompute cell metrics then re-enter AXIS 1 via
 *  resized() at lookAndFeelChanged()'s own tail — never write winsize
 *  directly. resized()'s tail calls @c session.start() UNGUARDED on every
 *  invocation — Session itself is idempotent (Source/terminal/Session.h),
 *  so no View-side first-positive-size guard survives here.
 *
 *  @par Zoom hot-reload (Direction A/B dual-tree dispatch)
 *  terminal::Model's own ID::zoom (Direction B, written by end::View's
 *  zoomIn/zoomOut/zoomReset actions) is hot-reload wired via a
 *  ValueTree::Listener on @ref model — registered in the events map
 *  (EventRegistration.cpp), dispatched through valueTreePropertyChanged()
 *  below, routing to lookAndFeelChanged(). The SAME listener also carries
 *  the jam::ID::screenDirty drain hookup (ARCHITECTURE.md's documented
 *  drain sequence): View calls Session::drain() then jam::CodeView::calc()
 *  to repaint — Session no longer self-listens for this (Session.h).
 *
 *  @par Input / Mouse — delegated encode paths
 *  Keyboard and mouse encode paths are owned by @ref input and @ref mouse
 *  respectively (Source/terminal/Input.h, Source/terminal/Mouse.h) — this
 *  class carries no encode logic of its own. keyPressed() is a one-line
 *  forward to @c input.sendKeyPress(). @ref mouse is a juce::MouseListener
 *  registered on codeView ADDITIVELY (@c codeView->addMouseListener(&mouse,
 *  false) in the constructor) — jam::CodeView's own ContentView still
 *  handles local text selection natively; this class carries no mouse
 *  overrides of its own.
 *
 *  @par Focus reporting (DEC private mode 1004)
 *  focusGained()/focusLost() call the PaneView base FIRST (it publishes
 *  ID::focus), then send the CSI I / CSI O focus-report sequences
 *  (jam::terminal::Sequence::focusIn / focusOut) to the host via
 *  session.writeInput() WHEN @ref model's focusEvents mode is set.
 */
class View
    : public end::PaneView
    , public juce::ValueTree::Listener
{
public:
    View (jam::UUID uuid, jam::Model& appModel, Session& sessionRef);

    ~View() override;

    Session& getSession() noexcept { return session; }

    /** @brief Width SSOT — computes cols/rows from component size,
     *  the reserved gutter (read LIVE from @ref lookAndFeel), and the
     *  Direction B cell size (read LIVE from @ref model, never cached);
     *  tells codeView the projection width and live-region row count.
     *  Scrollbar visibility never re-enters this computation (reserved
     *  gutter). Tail-calls @c session.start() UNGUARDED — Session's own
     *  idempotency (Session.h) makes every call after the first a
     *  correct no-op. */
    void resized() override;

    /** @brief Refreshes every LookAndFeel-derived visual: cell metrics
     *  (@ref lookAndFeel's getCodeMetrics(), applied to codeView, @ref mouse,
     *  and @ref model), cursor block (getCursorStyle()), and ligatures
     *  (getCodeLigatures()) — then tail-calls resized() so the new metrics
     *  take immediate effect. Invoked by JUCE on theme reload (end::View's
     *  own ID::theme handler cascades sendLookAndFeelChange() down the
     *  whole component tree) and once from the constructor to apply the
     *  initial style state. */
    void lookAndFeelChanged() override;

    /** @brief Single-key dispatch through the events map — property key takes
     *  priority; falls back to @p tree.getType() (mirrors end::View/
     *  end::LookAndFeel's established pattern). Listens on @ref model
     *  only — ID::zoom (routes to lookAndFeelChanged()) and
     *  jam::ID::screenDirty (routes to the drain + repaint sequence). */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief One-line forward to @c input.sendKeyPress() — keystrokes
     *  bypass the Model entirely; jam::TextModel is mutated ONLY by
     *  Session::drain().
     *  @return true when the key produced a non-empty sequence (consumed);
     *          false otherwise (JUCE falls through to any other handler). */
    bool keyPressed (const juce::KeyPress& key) override;

    /** @brief Publishes ID::focus via the PaneView base first, then sends
     *  the CSI I focus-gained report to the host WHEN @ref model's
     *  focusEvents mode is set. */
    void focusGained (FocusChangeType cause) override;

    /** @brief Publishes ID::focus via the PaneView base first, then sends
     *  the CSI O focus-lost report to the host WHEN @ref model's
     *  focusEvents mode is set. */
    void focusLost (FocusChangeType cause) override;

private:
    Session& session;

    /** @brief Direct terminal::Model reference — Demeter shortcut. The
     *  ctor's own parameter is named @c appModel (not @c model) precisely so
     *  it never shadows this member inside the constructor body — every
     *  Model read/write in this class goes through @c model directly; @ref
     *  session is reached only for its own non-Model verbs (getDocument(),
     *  start(), writeInput(), drain()). PaneView's own base-class @c model
     *  field (@c jam::Model&, a DIFFERENT type) is separately name-hidden by
     *  this member for the whole class — never reached unqualified here. */
    Model& model { session.getModel() };

    /** @brief Singleton LookAndFeel reference — source for every visual
     *  style/font/metric/ligature value this view applies. */
    end::LookAndFeel& lookAndFeel { *end::LookAndFeel::getInstance() };

    std::unique_ptr<jam::CodeView> codeView;

    /** @brief Owned keyboard encode path (Source/terminal/Input.h) —
     *  constructed against @ref model and @ref session, both already
     *  constructed above (declaration order). */
    Input input { model, session };

    /** @brief Owned mouse encode path (Source/terminal/Mouse.h) —
     *  registered on codeView ADDITIVELY in the constructor. */
    Mouse mouse { model, session };

    /** @brief Event dispatch map — keyed by juce::Identifier (property). Both
     *  entries listen on the @ref model tree — ID::zoom → lookAndFeelChanged();
     *  jam::ID::screenDirty → Session::drain() + jam::CodeView::calc().
     *  Dispatched via single-key lookup in valueTreePropertyChanged().
     *  Populated by registerEvents(). Defined in EventRegistration.cpp. */
    jam::Function::Map<juce::Identifier, void> events;

    /** @brief Populates the events map — see this class's own events doc
     *  comment. Defined in EventRegistration.cpp. */
    void registerEvents();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
