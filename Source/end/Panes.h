#pragma once
#include <JuceHeader.h>
#include "terminal/TerminalView.h"

// Per-tab split-pane container — IS the TAB tree in the model. Owns
// jam::PaneManager (adopts this same state, constructed with this component
// as its own container) — jam::PaneManager remains the sole author of every
// EDGE/PANE row, reference, value, and resizer bar. This class's own
// jam::PaneComponent pool is built directly by each verb below.
class Panes
    : public juce::Component
    , public jam::Model::Component
{
public:
    Panes (jam::UUID uuid, jam::Model& model);
    ~Panes() override;

    // state (paneManager.add) -> view (construct TerminalView from
    // paneManager.getPane, pool insert) -> focus -> resized.
    jam::UUID add();

    // state (paneManager.split, half-extent overload) -> view (construct
    // TerminalView from paneManager.getPane, pool insert) -> focus ->
    // resized.
    jam::UUID add (jam::UUID anchor, const juce::Identifier& edge);

    void resized() override;
    void visibilityChanged() override;

    void remove (jam::UUID uuid);

    // Moves keyboard focus to the adjacent pane in the given direction:
    // ID::paneLeft/paneRight/paneUp/paneDown.
    void focusPane (const juce::Identifier& direction);

    int getPaneCount() const noexcept;

    // Every PANE-leaf view this class builds IS a TerminalView — the add()
    // verbs above are the sole builders, so the static_cast is legitimate.
    TerminalView& get (jam::UUID uuid);

    // Deterministic fallback pane — first PANE-typed child in state's own
    // tree order (never pool iteration order).
    TerminalView& get();

private:
    jam::PaneManager paneManager;
    jam::HashMap<int64_t, std::unique_ptr<TerminalView>> panes;

    // Per-direction candidate/distance predicate — keyed by ID::paneLeft/
    // paneRight/paneUp/paneDown. Populated once by registerEvents().
    jam::Function::Map<juce::Identifier, std::pair<bool, int>> events;

    void registerEvents();

    TerminalView* findFocusedPane() const;

    TerminalView* findNearestPane (const juce::Identifier& direction,
                                   TerminalView* focused) const;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panes)
};
