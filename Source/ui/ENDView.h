/**
 * @file ENDView.h
 * @brief Application's root view: owns the sessions, registers the commands, and wires
 *        the config-driven graphics, window and mouse events. Hosts the message overlay and the Vulkan background.
 */
#pragma once
#include <JuceHeader.h>
#include "ui/SessionView.h"
#include "config/ConfigModel.h"
#include "lookAndFeel/ENDLookAndFeel.h"
#include "generated/Generated.h"
#include "plugins/Nexus.h"

/**
 * @class ENDView
 * @brief Root content component. It owns every SessionView, registers the commands in the `commands` map,
 *        and dispatches the changes of the model and the config to the graphics and mouse events.
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
     * @brief Registers the commands and the events, adds the background, and creates the parameters.
     *
     * The constructor queues an asynchronous call. The call sets the mouse configuration, adds the session views, and sets the key presses of the `keys` config.
     * It creates the first session when none exists, and invokes the new tab command when no tab is active. It takes no keyboard focus.
     */
    ENDView();

    /** @brief Stops listening to the model and the config. */
    ~ENDView() override;

    /** @brief Writes the view size to the state and lays out the background, the message overlay, and the active session view.
     *
     *  When a peer exists, adds messageOverlay to the desktop as a sibling peer of the native handle and bounds it to the view area.
     */
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
     * @brief Runs the callback that `commands` holds for the command id of the invocation.
     * @param info  Invocation that carries the command id.
     * @return True when `commands` holds the command id, and its callback has run.
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
    /** @brief Application root, which owns the sessions and the plugin graph. */
    Nexus& nexus { *Nexus::getInstance() };
    /** @brief Config of the application. */
    ConfigModel& config { *ConfigModel::getInstance() };

    /** @brief Registers the view's own Id::size parameter and the message overlay's, then sets the window size from the display config. */
    void createAndAttachParameters();

    /**
     * @brief Registers each family of commands into `commands`: application, tab, mode, split, load plugin, close, join, swap and resize.
     *
     * The profiler family is registered too when JAM_USING_PROFILER is set.
     */
    void registerActions();

    /** @brief Registers map::CommandId::quit, which asks the JUCE application to quit, and map::CommandId::reload, which reloads the config from its path. */
    void registerApplicationActions();

    /**
     * @brief Registers newTab, nextTab and prevTab.
     *
     * newTab reads the working directory of the focused pane of the active tab, adds a tab row to the focused session, then adds
     * a pane without an edge with that directory. The new pane takes the directory as its cwd, then loads the plugin of TabView::loadPlugin().
     * nextTab and prevTab move between the tabs of the focused session.
     */
    void registerTabActions();

    /**
     * @brief Registers splitHorizontal and splitVertical. Each adds a pane to the active tab at its edge.
     *
     * splitHorizontal uses the bottom edge, and splitVertical uses the right edge. Each command passes its edge and the
     * working directory of the focused pane to addPane(). TabView::addPane() then splits the focused pane at the edge, writes
     * the directory to the cwd of the new focused pane, and runs TabView::loadPlugin().
     */
    void registerSplitActions();

    /**
     * @brief Registers loadPlugin. The command calls TabView::loadPlugin() on the active tab.
     */
    void registerLoadPluginActions();

    /**
     * @brief Registers closeTerminal, closePane and closeTerminalPane. Each invokes the command on the focused pane of the active tab.
     *
     * The pane lists these commands in EditorView::getAllCommands(), and EditorView::perform() runs them.
     */
    void registerCloseActions();

    /**
     * @brief Registers one join command per join direction of TabView::getJoinCommands().
     *
     * A join calls TabView::join() on the active tab with the direction.
     */
    void registerJoinActions();

    /** @brief Registers swapLeft, swapDown, swapUp and swapRight, which swap the focused pane with its neighbour in that direction, when one exists. */
    void registerSwapActions();

    /**
     * @brief Registers reducePaneWidth, reducePaneHeight, expandPaneWidth and expandPaneHeight.
     *
     * Each resizes the focused pane along the width or the height, by the paneStep of the display config.
     */
    void registerResizeActions();

    /**
     * @brief Registers normal and open. Each sets the editor mode of the focused pane of the active tab.
     *
     * Each command passes its map::EditorMode value to EditorView::setMode().
     */
    void registerModeActions();

#if JAM_USING_PROFILER
    /**
     * @brief Sets the onPromptPainted callback of Nexus. The callback splits the active tab while it has fewer panes than the profiler table of the manifest file.
     *
     * The callback resets panePrompt first. Below the full count it invokes the splitVertical command. At the full count it invokes the quit command.
     */
    void registerProfilerActions();
#endif

    /**
     * @brief Adds a session view for each session row. It creates the first session when the sessions row has none.
     *
     * When the focused session has no active tab, the call invokes the new tab command.
     */
    void addSessions();

    /**
     * @brief Adds the view of a session, keyed by its identity, and attaches the view to the model.
     *
     * The focus of the view row is 1 when the view is the active session view, and 0 otherwise. The call then lays out the view.
     *
     * @param session  Session whose view is added.
     */
    void addSessionView (Session& session);

    /**
     * @brief Passes an edge to the active tab, which splits at the edge or adds a pane.
     *
     * With JAM_USING_PROFILER, the call first starts a profiler zone and the pane prompt.
     *
     * @param edge       Edge to split across: Id::left, Id::right, Id::top or Id::bottom. An invalid identifier adds a pane without a split.
     */
    void addPane (const juce::Identifier& edge);

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
    /** @brief Event callbacks keyed by the property, the tree type, or the parent tree type of the changed tree. `valueTreePropertyChanged()` looks the key up with `ConfigModel::getEventKey()`. */
    ConfigModel::Events events;
    /** @brief Member function of TabView that resizes a child by a step along an axis. The pane resize commands store `reducePane` and `expandPane`. */
    using PaneResize = void (TabView::*) (jam::UUID, const juce::Identifier&, float);
    /** @brief Inline capacity of a command callback. The capacity is the size of a tuple of the captures of the pane resize commands: `this`, an axis identifier and a PaneResize. The tuple size counts the closure padding. */
    static constexpr size_t commandCapacity { sizeof (std::tuple<ENDView*, juce::Identifier, PaneResize>) };
    /** @brief Command callbacks keyed by command id. The register functions add them, and perform() runs them. */
    jam::Function::Map<juce::CommandID, commandCapacity, void ()> commands;

    /** @brief Non-interactive message overlay, attached as a sibling desktop peer above the window. */
    jam::MessageOverlay messageOverlay;

#if JAM_USING_PROFILER
    std::optional<jam::Profiler::Zone> panePrompt;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ENDView)
};
