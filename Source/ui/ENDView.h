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
 *        to the graphics, window, and mouse events.
 */
class ENDView
    : public juce::Component
    , public jam::Model::Component<ENDView>
    , public juce::ValueTree::Listener
    , public juce::KeyListener
    , public juce::Value::Listener
    , public juce::ApplicationCommandTarget
{
public:
    /**
     * @brief Registers the actions, the events and the commands, creates the parameters and the message overlay.
     *
     * The constructor queues an asynchronous call. The call runs the events of the window and the mouse config,
     * runs the new-session action, and takes the keyboard focus.
     */
    ENDView();

    /** @brief Stops listening to the model, the config and the focused pane. */
    ~ENDView() override;

    /** @brief Writes the view size to the state, lays out the background, the message overlay and the active session view. */
    void resized() override;

    /**
     * @brief Gives a key press to the key press mappings of the command manager, then to its key sheet.
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

    /** @brief Mirrors the currently tracked TAB row's focusedPane onto the
     *  SESSIONS-level focusedPane parameter.
     *  @param value The changed Value — always focusedPane.
     */
    void valueChanged (juce::Value& value) override;

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
    /** @brief Shared instance of the `map::CommandId` map. The view reads the map through its `getInstance()`. */
    jam::SharedInstance<map::CommandId> commandId { std::in_place };
    /** @brief Command manager of the view. It holds the key presses and the key sheet from the keys config. */
    jam::SharedInstance<jam::CommandManager> commandManager { std::in_place };
    /** @brief Shared instance of the `map::MouseButton` map. */
    jam::SharedInstance<map::MouseButton> mouseButton { std::in_place };
    /** @brief Shared instance of the `map::Position` map. */
    jam::SharedInstance<map::Position> position { std::in_place };

    /** @brief Model that holds the window row, the sessions, and the overlay. */
    ENDModel& model { *ENDModel::getInstance() };
    /** @brief Action registry that the commands and the events run. */
    ENDActions& actions { *ENDActions::getInstance() };
    /** @brief Application root, which owns the sessions and the plugin graph. */
    Nexus& nexus { *Nexus::getInstance() };
    /** @brief Config of the application. */
    ConfigModel& config { *ConfigModel::getInstance() };
    /** @brief The LookAndFeel singleton of the application. */
    ENDLookAndFeel& endLookAndFeel { *ENDLookAndFeel::getInstance() };

    /** @brief Registers the view's own Id::size parameter and the message overlay's, then sets the window size from the display config. */
    void createAndAttachParameters();

    /** @brief Registers every action family: session, tab, zoom, pane, and window. */
    void registerActions();

    /** @brief Registers Id::newSession, which creates a session with a view, then runs the new-tab action. */
    void registerSessionActions();

    /** @brief Registers the tab, pane and split actions, tab navigation, close, reload and quit. */
    void registerTabActions();

    /**
     * @brief Registers zoomIn, zoomOut and zoomReset. Each moves the `Zoom` parameter of the host.
     *
     * With `exclusiveZoom` in the display config, only the plugin of the focused pane moves. Otherwise every plugin of the active session moves.
     * Zoom in and zoom out clamp the value to 0 through 1. Zoom reset sets the default value.
     */
    void registerZoomActions();

    /**
     * @brief Registers the pane focus, join, swap and resize actions for each direction, and the normal action.
     *
     * The normal action sets the `Mode` parameter of the plugin of the focused pane to the normal mode.
     */
    void registerPaneActions();

    /**
     * @brief Registers one action for each window position. The action of a leaf position toggles the visibility of its leaf.
     *        The center position has no toggle.
     */
    void registerWindowActions();

    /** @brief Registers the focusedPane conduit, the theme rebuild, the layout on a change of a leaf visibility, and every event family. */
    void registerEvents();

    /** @brief Registers the three config keys that feed setBackgroundParams(): background opacity, frame rate, and background resolution. */
    void registerGraphicsEvents();

    /** @brief Registers always-on-top and title-bar-button window events. */
    void registerWindowEvents();

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

    /** @brief Conduit to the focusedPane of the tab row that last changed. `valueChanged()` mirrors it to the sessions. */
    juce::Value focusedPane {};

    /** @brief Message overlay, shown above the sessions. */
    jam::MessageOverlay messageOverlay;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
