#include "end/Panes.h"
#include "Nexus.h"

Panes::Panes (jam::UUID uuid, jam::Model& m)
    : jam::Model::Component (*this, m, IDtype::tab, uuid)
    , paneManager (m, state, *this)
{
    setName (IDtype::tab.toString());
    model.createAndAddParameter<jam::ParameterText> (state, jam::ID::name, juce::String {});

    // Per-tab last-focused-pane memory (0 sentinel = none yet) — the SOLE
    // author is ENDView's own jam::ID::focus events-map reaction (EventRegistration.cpp);
    // Tabs::currentTabChanged reads it back to restore keyboard focus on tab switch.
    model.createAndAddParameter<jam::Parameter<int64_t>> (state, ID::focusedPane, int64_t { 0 });

    registerEvents();
}

Panes::~Panes() {}

//==============================================================================
jam::UUID Panes::add()
{
    jam::UUID uuid;
    auto view { std::make_unique<TerminalView> (uuid) };

    addAndMakeVisible (*view);

    auto* pane { view.get() };
    panes.try_emplace (uuid.value, std::move (view));

    pane->toFront (true);
    resized();

    return uuid;
}

jam::UUID Panes::add (jam::UUID anchor, const juce::Identifier& edge)
{
    return {};
}

//==============================================================================
void Panes::resized()
{
    for (auto& [key, view] : panes)
        view->setBounds (getLocalBounds());
}

void Panes::visibilityChanged()
{
    if (isShowing())
    {
        for (auto& [key, view] : panes)
            view->setVisible (true);

        resized();
    }
}

//==============================================================================
void Panes::remove (jam::UUID uuid)
{
    Nexus::getInstance()->getActiveSession().removeTerminal (uuid);
    panes.erase (uuid.value);
    resized();
}

//==============================================================================
void Panes::registerEvents() {}

//==============================================================================
TerminalView* Panes::findFocusedPane() const
{
    for (auto& [key, view] : panes)
        if (view->hasKeyboardFocus (true))
            return view.get();

    return nullptr;
}

TerminalView* Panes::findNearestPane (const juce::Identifier& direction,
                                      TerminalView* focused) const
{
    return nullptr;
}

//==============================================================================
void Panes::focusPane (const juce::Identifier& direction)
{
    auto* focused { findFocusedPane() };
    auto* nearest { findNearestPane (direction, focused) };

    if (focused != nullptr and nearest != nullptr)
        nearest->toFront (true);
}

//==============================================================================
int Panes::getPaneCount() const noexcept { return static_cast<int> (panes.size()); }

//==============================================================================
TerminalView& Panes::get (jam::UUID uuid)
{
    jassert (panes.contains (uuid.value));
    return *panes.at (uuid.value);
}

TerminalView& Panes::get()
{
    jassert (not panes.empty());
    return *panes.begin()->second;
}
