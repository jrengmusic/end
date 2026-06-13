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

    addAndMakeVisible (tabs);
    addChildComponent (messageOverlay);

    //==============================================================================
    attachments.add (std::make_unique<jam::Model::Attachment> (*this));
    attachments.add (std::make_unique<jam::Model::Attachment> (tabs));

    config.addListener (this);
    config.getLookAndFeel().addListener (this);

    setTabOrientation();
    tabs.addNewTab();

    //==============================================================================
    auto [width, height] = config.getInt (IDtype::window, ID::size);
    setSize (width, height);
}

View::~View()
{
    config.getLookAndFeel().removeListener (this);
    config.removeListener (this);
    removeKeyListener (this);
}

void View::resized()
{
    setViewState (getWidth(), getHeight());

    //==============================================================================
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
    if (property == ID::loadMessage)
        messageOverlay.showMessage (config.getLoadMessage());

    if (property == ID::orientation)
        setTabOrientation();

    if (jam::toBool (tree.getProperty (ID::focus)))
    {
        auto id { tree.getProperty (jam::ID::id) };
        state.setProperty (ID::focusedPane, id, nullptr);
    }

    if (tree.getType() == IDtype::graphics or tree.getType() == IDtype::tabButton)
        getTopLevelComponent()->sendLookAndFeelChange();
}

void View::valueTreeChildAdded (juce::ValueTree& parentTree,
                                juce::ValueTree& childWhichHasBeenAdded)
{
    if (parentTree.getType() == config.getLookAndFeel().getType())
        getTopLevelComponent()->sendLookAndFeelChange();

    auto id { childWhichHasBeenAdded.getChildWithName (IDtype::pane).getProperty (jam::ID::id) };
    state.setProperty (ID::focusedPane, id, nullptr);
}

//==============================================================================
void View::setViewState (int width, int height)
{
    state.setProperty (jam::ID::width, width, nullptr);
    state.setProperty (jam::ID::height, height, nullptr);
}

void View::setTabOrientation()
{
    auto display { config.getChildWithName (IDtype::display) };
    auto tabNode { display.getChildWithName (IDtype::tab) };
    auto pos { tabNode.getProperty (ID::orientation).toString() };

    if (Position::getInstance()->contains (pos))
        tabs.setOrientation (Position::get (pos));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
