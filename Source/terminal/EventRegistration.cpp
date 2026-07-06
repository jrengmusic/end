#include "terminal/View.h"

namespace terminal
{
/*____________________________________________________________________________*/

void View::registerEvents()
{
    // terminal::Model's own ID::zoom (Direction B, written by end::View's
    // zoomIn/zoomOut/zoomReset actions) — AXIS 2 recompute (this class's
    // single style-refresh entry point now covers cell metrics + cursor +
    // ligatures, not just font).
    events.add<juce::ValueTree&> (ID::zoom,
                                  [this] (juce::ValueTree&)
                                  {
                                      lookAndFeelChanged();
                                  });

    // Drain hookup (ARCHITECTURE.md's documented drain sequence) — View
    // calls Session::drain() then jam::CodeView::calc() to repaint.
    events.add<juce::ValueTree&> (jam::ID::screenDirty,
                                  [this] (juce::ValueTree&)
                                  {
                                      session.drain();
                                      codeView->calc();
                                  });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
