#include "end/ENDView.h"

ENDView::ENDView (jam::Model& m)
    : jam::Model::Component<ENDView> (m, m.getChildWithName (Id::toType (Id::window)))
    , messageOverlay (m, m.getChildWithName (Id::toType (Id::overlay)), [] {
        return juce::Font { juce::FontOptions()
                                .withName (ConfigModel::getInstance()->getRowValue (Id::toType (Id::overlay), Id::fontFamily).toString())
                                .withPointHeight (static_cast<float> (ConfigModel::getInstance()->getRowValue (Id::toType (Id::overlay), Id::textFontSize))) };
    })
{
    setOpaque (false);
    toFront (true);

    registerActions();
    registerEvents();

    addAndMakeVisible (background);
    addMouseListener (&background, true);

    addChildComponent (messageOverlay);

    createAndAttachParameters();

    focusedPane.addListener (this);
    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            auto display { config.getChildWithName (Id::toType (Id::display)) };
            auto alwaysOnTopRow { display.getChildWithName (Id::alwaysOnTop) };
            auto titleBarButtonsRow { display.getChildWithName (Id::titleBarButtons) };

            events.get (Id::useGpu, config.state);
            events.get (Id::alwaysOnTop, alwaysOnTopRow);
            events.get (Id::titleBarButtons, titleBarButtonsRow);
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
}

void ENDView::resized()
{
    setViewState (jam::Size<int16_t> (getWidth(), getHeight()));

    background.setBounds (getLocalBounds());
    messageOverlay.setBounds (getLocalBounds());

    auto* sessionView { getActiveSessionView() };

    if (sessionView != nullptr)
        sessionView->setBounds (getLocalBounds());
}

bool ENDView::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return actions.keyPressed (key);
}

void ENDView::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (events.contains (property))
    {
        events.get (property, tree);
    }
    else if (events.contains (tree.getType()))
    {
        events.get (tree.getType(), tree);
    }
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
    const auto windowSize { config.getRowValue (Id::toType (Id::display), Id::size) };
    const int width { windowSize[0] };
    const int height { windowSize[1] };

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
