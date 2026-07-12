#pragma once
#include <JuceHeader.h>
#include "end/SessionView.h"
#include "end/MessageOverlay.h"
#include "action/ENDActions.h"
#include "config/ConfigModel.h"
#include "../lookAndFeel/ENDLookAndFeel.h"
#include "Bimap.h"
#include "Nexus.h"

class ENDView
    : public juce::Component
    , public jam::Model::Component<ENDView>
    , public juce::ValueTree::Listener
    , public juce::KeyListener
    , public juce::Value::Listener
{
public:
    explicit ENDView (jam::Model& m);

    ~ENDView() override;

    void resized() override;

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    void valueChanged (juce::Value& value) override;

private:
    ENDActions& actions { *ENDActions::getInstance() };
    Nexus& nexus { *Nexus::getInstance() };
    ConfigModel& config { *ConfigModel::getInstance() };
    ENDLookAndFeel& endLookAndFeel { *ENDLookAndFeel::getInstance() };

    void createAndAttachParameters();

    void createDockPane (int positionKey);

    void registerActions();

    void registerEvents();

    void setBackground();

    void setBackgroundParams();

    void setPostProcess();

    void setPostProcessParams();

    void setMouseConfig();

    void setViewState (jam::Size<int16_t> size);

    SessionView* getActiveSessionView() noexcept;

    //==============================================================================
    jam::vulkan::ShaderComponent background;

    jam::HashMap<jam::UUID, std::unique_ptr<SessionView>> sessions;
    jam::HashMap<jam::UUID, std::unique_ptr<jam::Model::Attachment>> attachments;
    jam::Function::Map<juce::Identifier, void> events;

    juce::Value focusedPane {};

    MessageOverlay messageOverlay;

//==============================================================================
#if JUCE_DEBUG
    jam::debug::Widget widget { this, model.state, false };
#endif
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
