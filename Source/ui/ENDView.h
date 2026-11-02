/**
 * @file ENDView.h
 * @brief Application's own root view — owns sessions, wires actions and
 *        config-driven graphics/window/mouse events, and hosts the message
 *        overlay and Vulkan background.
 */
#pragma once
#include <JuceHeader.h>
#include "ui/SessionView.h"
#include "action/ENDActions.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"
#include "plugins/Nexus.h"

/**
 * @class ENDView
 * @brief Root content component. It owns every SessionView, registers the
 *        ENDActions, and dispatches the changes of the model and the config
 *        to the graphics and mouse events.
 */
class ENDView
    : public juce::Component
    , public jam::Model::Component<ENDView>
    , public juce::ValueTree::Listener
    , public juce::KeyListener
    , public juce::ApplicationCommandTarget
{
public:
    /**
     * @brief Registers the actions, the events and the commands, creates the parameters and the message overlay.
     *
     * The constructor queues an asynchronous call. The call runs the event of the `enabled` key of the config,
     * runs the new-session action, and takes the keyboard focus.
     */
    ENDView();

    /** @brief Stops listening to the model and the config. */
    ~ENDView() override;

    /** @brief Writes the view size to the state, lays out the background, the message overlay and the active session view. */
    void resized() override;

    /**
     * @brief Passes a key press to the command manager, which matches it against its key sheet.
     * @param key                  Key press.
     * @param originatingComponent Unused.
     * @return True when the command manager used the key press.
     */
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    /**
     * @brief Dispatches a changed property or tree type through the events map.
     *
     * When the changed tree is a row of the keys config, the command manager sets its key presses again.
     *
     * @param tree     The ValueTree whose property changed.
     * @param property The identifier of the changed property.
     */
    void
    valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    /** @brief Returns no next command target. The view is the root of the command targets. */
    juce::ApplicationCommandTarget* getNextCommandTarget() override;

    /**
     * @brief Adds the id of every command of the `map::CommandId` map.
     * @param commands  Array that receives the ids.
     */
    void getAllCommands (juce::Array<juce::CommandID>& commands) override;

    /**
     * @brief Describes a command by the name of its `map::CommandId` entry.
     *
     * The short name and the description are the name. The disabled flag is cleared.
     *
     * @param commandID  Command to describe.
     * @param result     Info to fill.
     */
    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;

    /**
     * @brief Runs the action that has the name of the command.
     * @param info  Invocation that carries the command id.
     * @return The result of the action run.
     */
    bool perform (const InvocationInfo& info) override;

    /**
     * @brief Sets the shader of the background, with the opacity, the resolution and the frame rate of the graphics config.
     * @param shader  Shader that the background draws.
     */
    void setBackground (std::unique_ptr<jam::VulkanShader> shader);

private:
    /** @brief Holds a `map::CommandId` instance, constructed in place. */
    jam::SharedInstance<map::CommandId> commandId { std::in_place };
    /** @brief Command manager of the view. It holds the key presses and the key sheet from the keys config. */
    jam::SharedInstance<jam::CommandManager> commandManager { std::in_place };
    /** @brief Holds a `map::MouseButton` instance, constructed in place. */
    jam::SharedInstance<map::MouseButton> mouseButton { std::in_place };

    /** @brief Model that holds the window row, the sessions, and the overlay. */
    ENDModel& model { *ENDModel::getInstance() };
    /** @brief Action registry that the commands and the events run. */
    ENDActions& actions { *ENDActions::getInstance() };
    /** @brief Application root, which owns the sessions and the plugin graph. */
    Nexus& nexus { *Nexus::getInstance() };
    /** @brief Config of the application. */
    ConfigModel& config { *ConfigModel::getInstance() };

    /** @brief Registers the view's own Id::size parameter and the message overlay's, then sets the window size from the display config. */
    void createAndAttachParameters();

    /** @brief Registers the session, tab, zoom and pane action families. */
    void registerActions();

    /** @brief Registers Id::newSession, which creates a session with a view, then runs the new-tab action. */
    void registerSessionActions();

    /** @brief Registers the tab, pane and split actions, tab navigation, close, reload and quit. */
    void registerTabActions();

    /**
     * @brief Registers zoomIn, zoomOut and zoomReset. Each moves the zoom parameter of the hosted plugins.
     *
     * The step is the `zoomStep` of the display config. With `exclusiveZoom` in the display config, the
     * focused pane of the active tab moves. Otherwise every pane of the active session moves.
     */
    void registerZoomActions();

    /**
     * @brief Registers the pane focus, join, swap and resize actions, and the normal action.
     *
     * The normal action sets the mode of the hosted plugin of the focused pane to the normal mode.
     */
    void registerPaneActions();

    /** @brief Registers the graphics events and the mouse events. */
    void registerEvents();

    /** @brief Registers the three config keys that feed setBackgroundParams(): background opacity, frame rate, and background resolution. */
    void registerGraphicsEvents();

    /** @brief Registers the enabled, imouse, orbit and reset mouse configuration events. */
    void registerMouseEvents();

    /** @brief Applies the background shader's opacity, resolution, and frame rate. */
    void setBackgroundParams();

    /** @brief Applies the interactive-mouse configuration to the background component. */
    void setMouseConfig();

    /** @brief Writes the view's current size onto state's Id::size property.
     *  @param size Current view size in pixels.
     */
    void setViewState (jam::Size<int16_t> size);

    /** @brief Returns the view of the focused session.
     *  @return The focused SessionView, or nullptr when none is focused.
     */
    SessionView* getActiveSessionView() noexcept;

    //==============================================================================
    /** @brief Background that draws the shader behind the sessions. */
    jam::VulkanShaderComponent background;

    /** @brief Session views keyed by session identity. Each view is owned here. */
    jam::HashMap<jam::UUID, std::unique_ptr<SessionView>> sessions;
    /** @brief Model attachments of the session views, keyed by session identity. */
    jam::HashMap<jam::UUID, std::unique_ptr<jam::Model::Attachment>> attachments;
    /** @brief Event callbacks keyed by the property or the tree type that changed. */
    jam::Function::Map<juce::Identifier, void> events;

    /** @brief Message overlay, shown above the sessions. */
    jam::MessageOverlay messageOverlay;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
