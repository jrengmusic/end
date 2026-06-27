/**
 * @file terminal/View.h
 * @brief Terminal view — PluginEditor analog. Holds Session reference.
 */
#pragma once
#include <JuceHeader.h>
#include "end/PaneView.h"
#include "terminal/Session.h"
#include "config/Config.h"

namespace terminal
{
/*____________________________________________________________________________*/

/** @brief Terminal pane view — PluginEditor analog.
 *
 *  PaneView subclass. Holds Session reference (owned by Nexus).
 *  Session outlives View — Nexus destroyed after window.
 *
 *  Phase 4: ValueTree::Listener on terminal::Model + config::Model,
 *           drain(), KeyListener, mouse events.
 */
class View : public end::PaneView
{
public:
    View (jam::UUID uuid, jam::Model& model, Session& sessionRef)
        : end::PaneView (uuid, model)
        , session (sessionRef)
    {}

    ~View() override = default;

    Session& getSession() noexcept { return session; }

    void resized() override {}

    // Phase 4: focusGained, keyPressed, mouse events
    // Phase 4: valueTreePropertyChanged, drain, config listener

private:
    Session& session;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (View)
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
