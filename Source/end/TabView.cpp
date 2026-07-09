#include "end/TabView.h"

TabView::TabView (jam::UUID uuid, jam::Model& m, juce::ValueTree tabsState)
    : jam::Model::Component (*this, m, tabsState, IDtype::tab, uuid)
    , paneManager (m, state, *this)
{
    setName (IDtype::tab.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});

    model.createAndAddParameter<jam::Parameter<int64_t>> (state, ID::focusedPane, int64_t { 0 });

    registerEvents();
}

TabView::~TabView() = default;

//==============================================================================
jam::UUID TabView::add()
{
    jam::UUID uuid;
    auto view { std::make_unique<TerminalView> (uuid) };

    addAndMakeVisible (*view);

    auto* pane { view.get() };
    panes.try_emplace (uuid, std::move (view));

    pane->toFront (true);
    resized();

    return uuid;
}

jam::UUID TabView::add (jam::UUID anchor, const juce::Identifier& edge)
{
    return {};
}

//==============================================================================
void TabView::resized()
{
    for (auto& [key, view] : panes)
        view->setBounds (getLocalBounds());
}

void TabView::visibilityChanged()
{
    if (isShowing())
    {
        for (auto& [key, view] : panes)
            view->setVisible (true);

        resized();
    }
}

//==============================================================================
void TabView::remove (jam::UUID uuid)
{
    panes.erase (uuid);
    resized();
}

//==============================================================================
void TabView::registerEvents() {}

//==============================================================================
TerminalView* TabView::findFocusedPane() const
{
    for (auto& [key, view] : panes)
        if (view->hasKeyboardFocus (true))
            return view.get();

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
int TabView::getPaneCount() const noexcept { return static_cast<int> (panes.size()); }

//==============================================================================
TerminalView& TabView::get (jam::UUID uuid)
{
    jassert (panes.contains (uuid));
    return *panes.at (uuid);
}

TerminalView& TabView::get()
{
    jassert (not panes.empty());
    return *panes.begin()->second;
}
