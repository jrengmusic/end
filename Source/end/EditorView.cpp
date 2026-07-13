#include "end/EditorView.h"

EditorView::EditorView (jam::Model& model, juce::ValueTree tabState, jam::UUID uuid)
    : jam::PaneComponent (model, tabState, IDtype::pane, uuid)
{
    model.createAndAddParameter<jam::Parameter<float>> (state, ID::zoom, defaultZoom);
}
