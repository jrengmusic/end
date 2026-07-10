#include "end/TabView.h"

TabView::TabView (jam::UUID uuid, jam::Model& m, juce::ValueTree sessionState)
    : jam::MatrixComponent (m, sessionState, IDtype::tab, uuid)
{
    setName (IDtype::tab.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});
}

//==============================================================================
jam::UUID TabView::add()
{
    jam::UUID uuid;
    MatrixComponent::add (uuid, std::make_unique<TerminalView> (model, state, uuid));
    get (uuid).grabKeyboardFocus();

    return uuid;
}

jam::UUID TabView::add (jam::UUID anchor, const juce::Identifier& edge)
{
    return {};
}

//==============================================================================
void TabView::remove (jam::UUID uuid)
{
    auto& pane { MatrixComponent::get (uuid) };
    state.removeChild (pane.getValueTree(), nullptr);
    MatrixComponent::remove (uuid);
}

//==============================================================================
TerminalView* TabView::findFocusedPane() const
{
    for (auto& [uuid, child] : getChildren())
        if (child->hasKeyboardFocus (true))
            return static_cast<TerminalView*> (child.get());

    return nullptr;
}

TerminalView* TabView::findNearestPane (const juce::Identifier& direction,
                                        TerminalView* focused) const
{
    return nullptr;
}

//==============================================================================
void TabView::focusPane (const juce::Identifier& direction)
{
    auto* focused { findFocusedPane() };
    auto* nearest { findNearestPane (direction, focused) };

    if (focused != nullptr and nearest != nullptr)
        nearest->toFront (true);
}

//==============================================================================
TerminalView& TabView::get (jam::UUID uuid)
{
    return static_cast<TerminalView&> (MatrixComponent::get (uuid));
}

TerminalView& TabView::get()
{
    jassert (getChildCount() > 0);
    auto& [uuid, child] { *getChildren().begin() };
    return static_cast<TerminalView&> (*child);
}
