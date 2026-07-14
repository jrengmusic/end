#include "end/ENDView.h"

ENDView::ENDView (jam::Model& m)
    : jam::Model::Component<ENDView> (m, m.getChildWithName (Id::toType (Id::window)))
    , messageOverlay (m, m.getChildWithName (Id::toType (Id::overlay)))
{
    setOpaque (false);
    addKeyListener (this);
    toFront (true);

    registerActions();
    registerEvents();

    addAndMakeVisible (background);
    addMouseListener (&background, true);

    addChildComponent (messageOverlay);

    createAndAttachParameters();

#if JUCE_DEBUG
    widget.setFormats (ConfigModel::getValidators());
#endif

    focusedPane.addListener (this);
    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            events.get (Id::useGpu, config.state);
            events.get (Id::alwaysOnTop, config.state);
            events.get (Id::titleBarButtons, config.state);
            events.get (Id::enabled, config.state);

            actions.run (Id::newSession);
            grabKeyboardFocus();
        });

    //==============================================================================
}

ENDView::~ENDView()
{
    model.removeListener (this);
    config.removeListener (this);
    focusedPane.removeListener (this);
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

void ENDView::valueChanged (juce::Value&)
{
    // focusedPane currently refers to whichever TAB row's focusedPane the
    // Id::focusedPane event last pointed it at — mirrors that value onto
    // the SESSIONS-level focusedPane parameter, a valid registered parameter
    // throughout.
    model.setValue (Id::toType (Id::sessions), Id::focusedPane, focusedPane.getValue());
}

void ENDView::createAndAttachParameters()
{
    auto [width, height] = config.getInt (Id::toType (Id::display), Id::size);

    //==============================================================================
    model.createAndAddParameter<jam::Parameter<int>> (
        state, Id::size, jam::Size<int16_t> (width, height).toInt());

    messageOverlay.registerParameters();

    //==============================================================================
    setSize (width, height);
}

void ENDView::setViewState (jam::Size<int16_t> size)
{
    state.setProperty (Id::size, size.toInt(), nullptr);
}

SessionView* ENDView::getActiveSessionView() noexcept
{
    auto* focusedSessionParameter { model.getParameter<jam::Parameter<int64_t>> (
        Id::toType (Id::sessions), Id::focusedSession) };
    jassert (focusedSessionParameter != nullptr);

    const jam::UUID sessionUuid { focusedSessionParameter->getValue() };

    return sessions.contains (sessionUuid) ? sessions.at (sessionUuid).get() : nullptr;
}
