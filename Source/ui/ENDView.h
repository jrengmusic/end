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

    /**
     * @brief Returns the active tab view as the next command target of the view.
     * @return The active tab view, or nullptr when no tab is active.
     */
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

    /**
     * @brief Registers every action family: session, application, tab, zoom, pane, join, swap, resize and terminal.
     *
     * The profiler family is registered too when JAM_USING_PROFILER is set.
     */
    void registerActions();

    /** @brief Registers Id::newSession, which creates a session with a view, then runs the new-tab action. */
    void registerSessionActions();

    /** @brief Registers Id::quit, which quits the application, and Id::reload, which reloads the config from its path. */
    void registerApplicationActions();

    /**
     * @brief Registers newTab, closeTab, nextTab and prevTab.
     *
     * newTab adds a tab row, then runs newPane without an edge. The new pane receives the directory of the focused
     * pane of the previously active tab as its cwd. newTab then loads EVE into the new pane.
     * closeTab removes the focused tab row of the focused session. nextTab and prevTab move between the tabs of the focused session.
     */
    void registerTabActions();

    /**
     * @brief Registers zoomIn, zoomOut and zoomReset. Each moves the zoom parameter of the hosted plugins.
     *
     * The step is the `zoomStep` of the display config. With `exclusiveZoom` in the display config, the
     * focused pane of the active tab moves. Otherwise every pane of the active session moves.
     */
    void registerZoomActions();

    /**
     * @brief Registers normal, newPane, loadPlugin, splitHorizontal, splitVertical and closePane.
     *
     * normal sets the mode of the hosted plugin of the focused pane to the normal mode.
     * newPane splits the focused pane at the given edge, or adds a pane when the edge is invalid.
     * loadPlugin writes the plugin file of the focused pane.
     * splitHorizontal and splitVertical split toward the bottom or the right, and load EVE into the new pane.
     * closePane removes the focused pane and its plugin. With one pane left, it closes the tab.
     */
    void registerPaneActions();

    /**
     * @brief Registers one join action per join direction of TabView::getJoinDirections().
     *
     * A join absorbs the neighbour pane. The plugin of the absorbed pane is removed from the focused session.
     */
    void registerJoinActions();

    /** @brief Registers swapLeft, swapDown, swapUp and swapRight, which swap the focused pane with its neighbour in that direction. */
    void registerSwapActions();

    /**
     * @brief Registers reducePaneWidth, reducePaneHeight, expandPaneWidth and expandPaneHeight.
     *
     * Each resizes the focused pane along the width or the height, by the paneStep of the display config.
     */
    void registerResizeActions();

    /**
     * @brief Registers the terminal reports of the panes, the ENAX mark and the start directory getter.
     *
     * Each terminal report writes its value to the row of its pane. The enax report sets the Id::enax property of the
     * row to true. The start directory getter copies the cwd of the row into the host buffer.
     */
    void registerTerminalActions();

    /**
     * @brief Returns the cwd of the focused pane of the active tab.
     * @return The directory, or an empty string when no tab is active.
     */
    juce::String getFocusedDirectory() const;

    /**
     * @brief Returns the row of a pane under the sessions row.
     *
     * The terminal reports and the host extensions pass the pane identity. The row must exist. The call asserts otherwise.
     *
     * @param pane  Identity of the pane.
     * @return The pane row.
     */
    juce::ValueTree getPaneRow (juce::int64 pane) const;

#if JAM_USING_PROFILER
    /**
     * @brief Registers promptPainted, which splits the active tab while it has fewer panes than the profiler table of the manifest file.
     *
     * When the tab has the full count, the action quits the application.
     */
    void registerProfilerActions();
#endif

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
    SessionView* getActiveSessionView() const noexcept;

    /**
     * @brief Returns the focused tab of the focused session.
     * @return The focused TabView, or nullptr when no session is focused, or when no tab of that session is focused.
     */
    TabView* getActiveTabView() const noexcept;

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

#if JAM_USING_PROFILER
    std::optional<jam::Profiler::Zone> panePrompt;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
