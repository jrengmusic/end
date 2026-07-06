#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View (jam::Model& m)
    : jam::Model::Component { m, IDtype::view }
    , tabs (m)
    , messageOverlay (m)
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);

    // Deep mouse listener (wantsEventsForAllNestedChildComponents = true) —
    // background sits beneath every other child, so a topmost pane normally
    // consumes its own mouse events before they ever reach background's own
    // mouseDown()/mouseDrag() overrides; this additionally observes the SAME
    // event stream, forwarded (never stolen) to background's orbit camera —
    // see mouseDown()/mouseDrag()'s own doc comments.
    addMouseListener (this, true);
    registerActions();
    registerEvents();
    createAndAttachParameters();

    addAndMakeVisible (background);
    addAndMakeVisible (tabs);
    addChildComponent (messageOverlay);

    config.addListener (this);
    model.addListener (this);

    juce::MessageManager::callAsync (
        [this]
        {
            events.get (ID::gpu, config.state);
            events.get (ID::alwaysOnTop, config.state);
            events.get (ID::titleBarButtons, config.state);
            events.get (jam::ID::enabled, config.state);

            tabs.addNewTab();
            grabKeyboardFocus();
        });

    //==============================================================================
}

View::~View()
{
    model.removeListener (this);
    config.removeListener (this);
    removeKeyListener (this);
}

void View::resized()
{
    setViewState (jam::Size<int16_t> (getWidth(), getHeight()));

    background.setBounds (getLocalBounds());
    tabs.setBounds (getLocalBounds());
    messageOverlay.setBounds (getLocalBounds());
}

bool View::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return registry.keyPressed (key);
}

void View::mouseDown (const juce::MouseEvent& e)
{
    lastOrbitDragPosition = e.position;

    if (mouseEnabled and jam::map::MouseButton::isDown (e.mods, resetButtonConfig))
        resetButtonDragged = false;
}

void View::mouseDrag (const juce::MouseEvent& e)
{
    if (mouseEnabled)
    {
        if (background.hasMesh() and jam::map::MouseButton::isDown (e.mods, orbitButtonConfig))
            background.addOrbitDelta (
                e.position.x - lastOrbitDragPosition.x, e.position.y - lastOrbitDragPosition.y);

        // Tracked independently of the orbit branch above — orbit and reset
        // may be configured to different buttons (RATIFIED SCHEMA), so a
        // drag of the reset button alone (not the orbit button) must still
        // disqualify mouseUp()'s own click-reset below.
        if (jam::map::MouseButton::isDown (e.mods, resetButtonConfig))
            resetButtonDragged = true;
    }

    lastOrbitDragPosition = e.position;
}

void View::mouseUp (const juce::MouseEvent& e)
{
    if (mouseEnabled and background.hasMesh()
        and jam::map::MouseButton::isDown (e.mods, resetButtonConfig) and not resetButtonDragged)
        background.resetCamera();
}

void View::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& details)
{
    if (mouseEnabled and background.hasMesh())
        background.addZoomDelta (details.deltaY);
}

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);

    sendLookAndFeelChange();
}

void View::valueTreeChildAdded (juce::ValueTree& parentTree,
                                juce::ValueTree& childWhichHasBeenAdded)
{
    auto id { childWhichHasBeenAdded.getChildWithName (IDtype::pane).getProperty (jam::ID::id) };
    state.setProperty (ID::focusedPane, id, nullptr);
}

void View::createAndAttachParameters()
{
    auto [width, height] = config.getInt (IDtype::display, ID::size);

    //==============================================================================
    model.createAndAddParameter<jam::Parameter<int>> (
        state, ID::size, jam::Size<int16_t> (width, height).toInt());

    attachments.add (std::make_unique<jam::Model::Attachment> (*this));
    attachments.add (std::make_unique<jam::Model::Attachment> (tabs));
    attachments.add (std::make_unique<jam::Model::Attachment> (messageOverlay));

    messageOverlay.registerParameters();

    //==============================================================================
    setSize (width, height);
}

void View::setViewState (jam::Size<int16_t> size)
{
    state.setProperty (ID::size, size.toInt(), nullptr);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
