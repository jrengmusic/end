#include "end/ENDView.h"

ENDView::ENDView (jam::Model& m)
    : jam::Model::Component<ENDView> (m, m.getChildWithName (IDtype::window))
    , messageOverlay (m, m.getChildWithName (IDtype::overlay))
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);

    registerActions();
    registerEvents();

    addAndMakeVisible (background);
    addMouseListener (&background, true);

    addChildComponent (messageOverlay);

    createAndAttachParameters();

#if JUCE_DEBUG
    widget.setFormats (ConfigModel::validators);
#endif

    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            events.get (ID::gpu, config.state);
            events.get (ID::alwaysOnTop, config.state);
            events.get (ID::titleBarButtons, config.state);
            events.get (jam::ID::enabled, config.state);

            actions.run (ID::newSession);
            grabKeyboardFocus();
        });

    //==============================================================================
}

ENDView::~ENDView()
{
    model.removeListener (this);
    config.removeListener (this);
    removeKeyListener (this);
}

void ENDView::resized()
{
    setViewState (jam::Size<int16_t> (getWidth(), getHeight()));

    background.setBounds (getLocalBounds());
    messageOverlay.setBounds (getLocalBounds());

    if (auto* sessionView { getActiveSessionView() })
        sessionView->setBounds (getLocalBounds());
}

bool ENDView::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return actions.keyPressed (key);
}

void ENDView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

void ENDView::createAndAttachParameters()
{
    auto [width, height] = config.getInt (IDtype::display, ID::size);

    //==============================================================================
    model.createAndAddParameter<jam::Parameter<int>> (
        state, ID::size, jam::Size<int16_t> (width, height).toInt());

    messageOverlay.registerParameters();

    //==============================================================================
    setSize (width, height);
}

void ENDView::setViewState (jam::Size<int16_t> size)
{
    state.setProperty (ID::size, size.toInt(), nullptr);
}

SessionView* ENDView::getActiveSessionView() noexcept
{
    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        IDtype::sessions, ID::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    const jam::UUID sessionUuid { focusedSessionParameter->getValue() };

    return sessions.contains (sessionUuid) ? sessions.at (sessionUuid).get() : nullptr;
}
