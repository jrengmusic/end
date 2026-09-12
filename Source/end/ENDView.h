/**
 * @file ENDView.h
 * @brief Application's own root view — owns sessions, wires actions and
 *        config-driven graphics/window/mouse events, and hosts the message
 *        overlay and Vulkan background.
 */
#pragma once
#include <JuceHeader.h>
#include "end/SessionView.h"
#include "end/MessageOverlay.h"
#include "action/ENDActions.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"
#include "Nexus.h"

/**
 * @class ENDView
 * @brief Root content component — owns every SessionView, registers all
 *        ENDActions, and dispatches config/model ValueTree changes to
 *        graphics, window, and mouse event handlers.
 */
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

    /** @brief Forwards a key press to ENDActions for dispatch.
     *  @param key                  Key press to dispatch.
     *  @param originatingComponent Unused — ENDActions dispatch is global.
     *  @return True when ENDActions consumed the key press.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    /** @brief Dispatches a changed property or tree type through the events map.
     *  @param tree     The ValueTree whose property changed.
     *  @param property The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Mirrors the currently tracked TAB row's focusedPane onto the
     *  SESSIONS-level focusedPane parameter.
     *  @param value The changed Value — always focusedPane.
     */
    void valueChanged (juce::Value& value) override;

private:
    ENDActions& actions { *ENDActions::getInstance() };
    Nexus& nexus { *Nexus::getInstance() };
    ConfigModel& config { *ConfigModel::getInstance() };
    ENDLookAndFeel& endLookAndFeel { *ENDLookAndFeel::getInstance() };

    /** @brief Registers the view's own Id::size parameter and the message overlay's. */
    void createAndAttachParameters();

    /** @brief Registers every action family (session, tab, zoom, pane, window). */
    void registerActions();

    /** @brief Registers Id::newSession — mints a session and its first tab. */
    void registerSessionActions();

    /** @brief Registers new/close-tab, new/split-pane, plugin creation, reload, and quit actions. */
    void registerTabActions();

    /** @brief Registers zoomIn/zoomOut/zoomReset against the focused pane's zoom parameter. */
    void registerZoomActions();

    /** @brief Registers pane focus, join, swap, and resize actions for each direction. */
    void registerPaneActions();

    /** @brief Registers dock-pane visibility toggles for each WINDOW leaf. */
    void registerWindowActions();

    /** @brief Registers the focusedPane conduit, theme rebuild, and every event family. */
    void registerEvents();

    /** @brief Registers background/post-process shader and parameter events. */
    void registerGraphicsEvents();

    /** @brief Registers always-on-top and title-bar-button window events. */
    void registerWindowEvents();

    /** @brief Registers imouse/orbit/reset mouse configuration events. */
    void registerMouseEvents();

    /** @brief Compiles and applies the active background shader from config. */
    void setBackground();

    /** @brief Applies the background shader's opacity, resolution, and frame rate. */
    void setBackgroundParams();

    /** @brief Compiles and applies the active post-process shader from config. */
    void setPostProcess();

    /** @brief Applies the post-process shader's opacity and resolution. */
    void setPostProcessParams();

    /** @brief Applies the interactive-mouse configuration to the background component. */
    void setMouseConfig();

    /** @brief Writes the view's current size onto state's Id::size property.
     *  @param size Current view size in pixels.
     */
    void setViewState (jam::Size<int16_t> size);

    /** @brief Resolves the currently focused session's own view.
     *  @return The focused SessionView, or nullptr when none is focused.
     */
    SessionView* getActiveSessionView() noexcept;

    //==============================================================================
    jam::VulkanShaderComponent background;

    jam::HashMap<jam::UUID, std::unique_ptr<SessionView>> sessions;
    jam::HashMap<jam::UUID, std::unique_ptr<jam::Model::Attachment>> attachments;
    jam::Function::Map<juce::Identifier, void> events;

    juce::Value focusedPane {};

    MessageOverlay messageOverlay;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
