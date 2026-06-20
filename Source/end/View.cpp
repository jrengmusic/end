#include "end/View.h"

namespace end
{
/*____________________________________________________________________________*/

View::View (jam::Model& m)
    : jam::Model::Component { m, IDtype::view }
    , tabs (m)
{
    setOpaque (false);
    addKeyListener (this);
    setWantsKeyboardFocus (true);
    toFront (true);
    registerActions();
    registerEvents();

    addAndMakeVisible (tabs);
    addChildComponent (messageOverlay);

    auto [width, height] = config.getInt (IDtype::config, ID::size);
    state.setProperty (jam::ID::width, width, nullptr);
    state.setProperty (jam::ID::height, height, nullptr);

    //==============================================================================
    attachments.add (std::make_unique<jam::Model::Attachment> (*this));
    attachments.add (std::make_unique<jam::Model::Attachment> (tabs));

    config.addListener (this);
    model.addListener (this);

    setTabOrientation();
    tabs.addNewTab();

    //==============================================================================
    setSize (width, height);

    juce::MessageManager::callAsync (
        [this]
        {
            // Fire window events for initial state — addListener doesn't replay.
            // Uses the SAME event handlers as VTPC hot-reload (DRY/SSOT).
            events.get (ID::gpu, config.state);
            events.get (ID::alwaysOnTop, config.state);
            events.get (ID::titleBarButtons, config.state);
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
    setViewState (getWidth(), getHeight());

    tabs.setBounds (getLocalBounds());
    messageOverlay.setBounds (getLocalBounds());
}

bool View::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused (originatingComponent);
    return registry.keyPressed (key);
}

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

void View::valueTreeChildAdded (juce::ValueTree& parentTree,
                                juce::ValueTree& childWhichHasBeenAdded)
{
    auto id { childWhichHasBeenAdded.getChildWithName (IDtype::pane).getProperty (jam::ID::id) };
    state.setProperty (ID::focusedPane, id, nullptr);
}

void View::setViewState (int width, int height)
{
    model.setValue (IDtype::view, jam::ID::width, width);
    model.setValue (IDtype::view, jam::ID::height, height);
}

void View::setTabOrientation()
{
    auto pos { config.state.getProperty (ID::tabOrientation).toString() };

    if (Position::getInstance()->contains (pos))
        tabs.setOrientation (Position::get (pos));
}

void View::parentHierarchyChanged() {}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
