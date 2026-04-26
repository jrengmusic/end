/**
 * @file ActionList.h
 * @brief Command palette — self-contained component with fuzzy search and action list.
 */

#pragma once

#include <JuceHeader.h>
#include "../lua/Engine.h"
#include "../component/MessageOverlay.h"
#include "Action.h"
#include "ActionRow.h"
#include "KeyHandler.h"

namespace Action
{ /*____________________________________________________________________________*/

/**
 * @class List
 * @brief Command palette component with fuzzy search and keyboard navigation.
 *
 * Self-contained: builds rows from the Action::Registry on construction,
 * manages selection state, and handles all keyboard input through KeyHandler.
 * Hosted inside a Terminal::ModalWindow via Popup::show(). Escape is not
 * consumed — it falls through to the hosting ModalWindow for dismissal.
 * State changes (binding edits, dirty flag) propagate via juce::ValueTree::Listener.
 *
 * @see Action::Registry
 * @see Action::Row
 * @see Action::KeyHandler
 */
class List
    : public jam::NativeContextResource
    , private juce::ValueTree::Listener
{
public:
    /** @brief Constructs the command palette and builds all action rows from Registry.
     *
     * Computes size from Config proportions, builds search and action rows,
     * configures fonts and colours. Ready to be hosted in a Terminal::ModalWindow
     * via Popup::show().
     *
     * @param main    The main component used for size calculations; L&F inherited from this.
     * @param engine  Lua engine for key lookup and patching. Must outlive the List.
     */
    List (juce::Component& main, lua::Engine& engine);

    /** @brief Destructor. Reloads Config if bindings were modified during the session. */
    ~List() override;

    /** @brief Routes key presses to KeyHandler or binding mode handler.
     *
     * Escape in navigation mode calls onDismiss; in search mode either selects
     * the first visible result or clears the search text.
     *
     * @param key  The key press event to handle.
     * @return     True if the key was consumed, false to propagate up.
     */
    bool keyPressed (const juce::KeyPress& key) override;

    /** @brief Lays out the search box, viewport, and message overlay within current bounds. */
    void resized() override;

    /** @brief Called after an action is executed; used for close-on-run behavior. */
    std::function<void()> onActionRun;

    /** @brief Called when the user dismisses the action list via Escape from selection mode. */
    std::function<void()> onDismiss;

private:
    /** @brief Minimum width in logical pixels. */
    static constexpr int minimumWidth { 600 };

    /** @brief Minimum height in logical pixels. */
    static constexpr int minimumHeight { 400 };

    /** @brief Fixed height in pixels for a standard action row. */
    static constexpr int rowHeight { 28 };

    /** @brief Fixed height in pixels for a separator row. */
    static constexpr int separatorRowHeight { 12 };

    /** @brief Timeout in milliseconds before binding mode auto-exits. */
    static constexpr int bindingModeTimeoutMs { 60000 };

    /** @brief ValueTree property key for the currently bound row index (-1 = none). */
    static const juce::Identifier bindingRowIndexId;

    /** @brief ValueTree property key for the bindings-dirty flag (triggers Config reload on close). */
    static const juce::Identifier bindingsDirtyId;

    /** @brief Reference to the main component; used for size calculations. */
    juce::Component& main;

    /** @brief Lua engine reference; used for key lookup and patching. */
    lua::Engine& luaEngine;

    /** @brief ValueTree root holding ACTION nodes and binding state. */
    jam::ValueTree state { "ACTION_LIST" };

    /** @brief Scrollable viewport hosting the rowContainer. */
    juce::Viewport viewport;

    /** @brief Container for all action rows (excluding the search row at index 0). */
    juce::Component rowContainer;

    /** @brief Owned list of all rows: index 0 is the search box, 1..N are action rows. */
    jam::Owner<Action::Row> rows;

    /** @brief Overlay that shows transient messages (e.g. binding mode prompt). */
    MessageOverlay messageOverlay;

    /** @brief Keyboard dispatch handler built from navigation and search tables. */
    std::optional<KeyHandler> keyHandler;

    /** @brief Builds and populates all rows from Action::Registry.
     *
     * Inserts: search row (index 0), global action rows, separator, prefix key row,
     * then modal action rows. Calls layoutRows() on completion.
     */
    void buildRows();

    /** @brief Applies font, colour, and input settings to the search TextEditor.
     *
     * @param editor  The TextEditor widget inside the search Row.
     */
    void configureSearchBox (juce::TextEditor& editor);

    /** @brief Applies highlight colour and label colours to an action Row.
     *
     * @param row  The action row to style.
     */
    void configureActionRow (Row& row);

    /** @brief Filters visible rows by fuzzy-matching against @p query.
     *
     * Empty query restores all rows. Non-empty query hides non-matching rows
     * (separators are always hidden during search). Calls layoutRows() and selectRow(0).
     *
     * @param query  The search string typed in the search box.
     */
    void filterRows (const juce::String& query);

    /** @brief Recomputes row positions and resizes rowContainer to fit visible rows. */
    void layoutRows();

    /** @brief Resolves the target row index, skipping non-selectable or non-visible rows.
     *
     * Scans in the direction of travel from @p index. Falls back to 0 (search box)
     * if no valid row is found.
     *
     * @param index  The requested row index.
     * @return       The resolved selectable, visible row index.
     */
    int findSelectableRow (int index) const;

    /** @brief Selects the row at @p index, scrolling into view as needed.
     *
     * Skips non-selectable or non-visible rows (e.g. separator, filtered-out actions)
     * by stepping in the direction of travel.
     * Clamps to valid range. Grabs keyboard focus when a navigation row is selected.
     *
     * @param index  The target row index to select.
     */
    void selectRow (int index);

    /** @brief Executes the action on the currently selected row.
     *
     * If row 0 (search) is selected, finds the first visible action row and runs it.
     */
    void executeSelected();

    /** @brief Returns the count of visible, selectable rows (excludes search row and separators).
     *
     * @return Number of rows the user can navigate to.
     */
    int visibleRowCount() const noexcept;

    /** @brief Returns the index of the currently selected row.
     *
     * @return Selected row index; 0 if no navigation row is selected.
     */
    int getSelectedIndex() const noexcept;

    /** @brief Returns the current highlight colour from Config.
     *
     * @return Highlight colour for selected rows.
     */
    juce::Colour getHighlightColour() const;

    /** @brief Returns the row index currently in binding mode, or -1 if none.
     *
     * @return Current binding row index.
     */
    int getBindingRowIndex() const;

    /** @brief Stores @p index as the active binding row in the ValueTree.
     *
     * @param index  Row index to put into binding mode; -1 to clear.
     */
    void setBindingRowIndex (int index);

    /** @brief Enters binding mode for the currently selected row, if selectable.
     *
     * Shows the MessageOverlay prompt and stores the row index via setBindingRowIndex().
     */
    void enterBindingMode();

    /** @brief Exits binding mode, hides the MessageOverlay, and restores keyboard focus. */
    void exitBindingMode();

    /** @brief Handles a key press while in binding mode.
     *
     * Ctrl+C exits binding mode without remapping. Any other key is recorded as
     * the new shortcut for the bound row, patches Config, and rebuilds the key map.
     *
     * @param key  The key press to record or cancel with.
     * @return     True if the key was consumed by binding mode.
     */
    bool handleBindingKey (const juce::KeyPress& key);

    /** @brief Responds to ValueTree property changes — patches Config and rebuilds key map.
     *
     * Called by JUCE when any property on the ACTION_LIST tree changes. Compares
     * each action row's current shortcut label against Config; patches any divergence
     * and marks bindingsDirty for Config reload on destruction.
     *
     * @param tree      The ValueTree that changed.
     * @param property  The identifier of the property that changed.
     */
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (List)
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Action
