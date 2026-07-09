#include "end/ENDView.h"
#include "Nexus.h"

ENDView::ENDView (jam::Model& m)
    : jam::Model::Component (*this, m, m.getChildWithName (IDtype::window))
    , paneManager (m, state, *this)
    , tabs (m, Nexus::getInstance()->getActiveSession().state.getChildWithName (IDtype::tabs))
    , messageOverlay (m, m.getChildWithName (IDtype::overlay))
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);

    registerActions();
    registerEvents();

    addAndMakeVisible (background);
    // Background listens to its own owner's subtree directly -- its own
    // mouseDown()/mouseDrag()/mouseUp()/mouseWheelMove() speak straight to
    // the shader's own gesture machine, no forwarding hop through this View.
    addMouseListener (&background, true);
    addAndMakeVisible (tabs);
    addChildComponent (messageOverlay);

    createAndAttachParameters();

    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            events.get (ID::gpu, config.state);
            events.get (ID::alwaysOnTop, config.state);
            events.get (ID::titleBarButtons, config.state);
            events.get (jam::ID::enabled, config.state);

            registry.run (ID::newTab);
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
    tabs.setBounds (getLocalBounds());

    paneManager.layout (getLocalBounds(), components);
}

bool ENDView::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return registry.keyPressed (key);
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
