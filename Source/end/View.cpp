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
    registerActions();
    registerEvents();
    createAndAttachParameters();

    // background is added FIRST — JUCE paints children in child-index order
    // (Component::paintComponentAndChildren()'s getChildren() walk), and
    // addChildComponent() appends new children at the end of that list, so
    // the first-added child lands at index 0 and paints before every
    // subsequently-added sibling — the rearmost position, with no explicit
    // toBack() needed.
    addAndMakeVisible (background);
    addAndMakeVisible (tabs);
    addChildComponent (messageOverlay);

    config.addListener (this);
    model.addListener (this);

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
    setTabOrientation();
    tabs.addNewTab();
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

void View::setTabOrientation()
{
    auto pos { config.state.getProperty (ID::tabOrientation).toString() };

    if (Position::getInstance()->contains (pos))
        tabs.setOrientation (Position::get (pos));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
