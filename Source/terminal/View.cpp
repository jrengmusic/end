#include "terminal/View.h"

namespace terminal
{
/*____________________________________________________________________________*/

View::View (jam::UUID uuid, jam::Model& model, Session& sessionRef)
    : end::PaneView (uuid, model)
    , session (sessionRef)
{
    codeView = std::make_unique<jam::CodeView> (session.getDocument());
    addAndMakeVisible (codeView.get());

    registerEvents();
}

View::~View() {}

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

void View::resized() { auto area { getLocalBounds() }; }

bool View::keyPressed (const juce::KeyPress& key) { return false; }

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
