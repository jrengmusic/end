/**
 * @file ActionList.h
 * @brief Fuzzy-searchable command palette overlay for Terminal::Action entries.
 *
 * `Terminal::ActionList` is a `juce::Component` child of `MainComponent` that
 * presents a searchable list of all registered actions.  It is lazily created
 * on first trigger and reused on subsequent invocations.
 *
 * ### Layout
 * ```
 * ┌──────────────────────────────────────────────────┐
 * │ [search text box                               ] │
 * ├──────────────────────────────────────────────────┤
 * │ Copy          Copy selection to clipboard  ctrl+c│
 * │ Paste         Paste from clipboard         ctrl+v│
 * │ Quit          Quit application             ctrl+q│
 * └──────────────────────────────────────────────────┘
 * ```
 *
 * ### Positioning
 * Controlled by `Config::Key::keysActionListPosition` (`"top"` or `"bottom"`).
 * Width is 60 % of the parent, centred horizontally.
 *
 * ### Fuzzy search
 * On every keystroke `jreng::FuzzySearch::getResult()` is called against a
 * `Data::vector` of action names built when `show()` is called.  An empty
 * query shows all entries.
 *
 * @note All methods must be called on the **MESSAGE THREAD**.
 *
 * @see Terminal::Action
 * @see Config::Key::keysActionListPosition
 * @see jreng::FuzzySearch
 */

#pragma once

#include <JuceHeader.h>
#include "Action.h"

namespace Terminal
{

/**
 * @class ActionList
 * @brief Fuzzy-searchable command palette component.
 *
 * Implements `juce::ListBoxModel` directly so no separate model object is
 * needed.  Implements `juce::TextEditor::Listener` to react to search input
 * and key events.
 *
 * @par Thread context
 * **MESSAGE THREAD** — all public methods.
 *
 * @see Terminal::Action::getEntries
 */
class ActionList : public juce::Component,
                   private juce::TextEditor::Listener,
                   private juce::ListBoxModel
{
public:
    //==========================================================================
    /**
     * @brief Constructs the ActionList, creating the search box and result list
     *        as child components.
     *
     * The component starts hidden.  Call `show()` to make it visible.
     */
    ActionList();

    /** @brief Default destructor. */
    ~ActionList() override = default;

    //==========================================================================
    /**
     * @brief Makes the palette visible, positions it within @p parent, and
     *        populates the list from `Action::getEntries()`.
     *
     * Reads `Config::Key::keysActionListPosition` to decide whether to anchor
     * to the top or bottom of @p parent.  Grabs keyboard focus to the search
     * text editor immediately.
     *
     * @param parent  The component to position this palette within (typically
     *                `MainComponent`).
     */
    void show (juce::Component& parent);

    /**
     * @brief Hides the palette and clears the search text.
     *
     * Returns keyboard focus to the parent component.
     */
    void hide();

    /**
     * @brief Returns whether the palette is currently visible.
     *
     * @return `true` if the palette is visible and active.
     */
    bool isActive() const noexcept;

    //==========================================================================
    /** @brief Lays out the search box and list box within the current bounds. */
    void resized() override;

    /** @brief Paints the semi-transparent background. */
    void paint (juce::Graphics& g) override;

    /** @brief Intercepts arrow keys to forward navigation to the list box. */
    bool keyPressed (const juce::KeyPress& key) override;

private:
    //==========================================================================
    // juce::TextEditor::Listener
    //==========================================================================

    /** @brief Filters entries on every keystroke. */
    void textEditorTextChanged (juce::TextEditor&) override;

    /** @brief Executes the selected entry and hides the palette. */
    void textEditorReturnKeyPressed (juce::TextEditor&) override;

    /** @brief Hides the palette without executing anything. */
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

    //==========================================================================
    // juce::ListBoxModel
    //==========================================================================

    /** @brief Returns the number of currently filtered rows. */
    int getNumRows() override;

    /**
     * @brief Paints a single row with three columns: name, description, shortcut.
     *
     * @param rowNumber        Zero-based row index.
     * @param g                Graphics context for this row.
     * @param width            Row width in pixels.
     * @param height           Row height in pixels.
     * @param rowIsSelected    Whether this row is currently selected.
     */
    void paintListBoxItem (int rowNumber,
                           juce::Graphics& g,
                           int width,
                           int height,
                           bool rowIsSelected) override;

    /** @brief Executes the clicked entry and hides the palette. */
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    //==========================================================================
    // Internal helpers
    //==========================================================================

    /**
     * @brief Rebuilds `filteredIndices` from the current search query.
     *
     * An empty query shows all entries.  Results are sorted by fuzzy relevance.
     *
     * @param query  The current text from the search box.
     */
    void filterEntries (const juce::String& query);

    /**
     * @brief Executes the entry at `filteredIndices[row]` if the index is valid.
     *
     * @param row  Zero-based index into `filteredIndices`.
     */
    void executeRow (int row);

    /**
     * @brief Computes and applies the bounds of this component within @p parent.
     *
     * @param parent    The parent component.
     * @param position  `"top"` or `"bottom"` from Config.
     */
    void applyBounds (juce::Component& parent, const juce::String& position);

    //==========================================================================
    // Child components
    //==========================================================================

    /** @brief Single-line search input at the top of the palette. */
    juce::TextEditor searchBox;

    /** @brief Scrollable list of filtered action entries. */
    juce::ListBox resultList;

    //==========================================================================
    // Data
    //==========================================================================

    /**
     * @brief Snapshot of all registered entries, captured in `show()`.
     *
     * Stored as a copy so the palette is not affected by mid-session
     * `Action::reload()` calls while the palette is open.
     */
    std::vector<Action::Entry> entries;

    /**
     * @brief Indices into `entries` for the currently visible (filtered) rows.
     *
     * Rebuilt by `filterEntries()` on every keystroke.
     */
    std::vector<int> filteredIndices;

    /**
     * @brief Dataset of entry names passed to `jreng::FuzzySearch::getResult()`.
     *
     * Rebuilt from `entries` in `show()`.
     */
    jreng::FuzzySearch::Data::vector searchDataset;

    /** @brief Cached parent pointer for focus restoration on hide(). */
    juce::Component* parentComponent { nullptr };

    //==========================================================================
    // Layout constants
    //==========================================================================

    /** @brief Fixed height of the search text box in pixels. */
    static constexpr int searchBoxHeight { 30 };

    /** @brief Height of each row in the result list in pixels. */
    static constexpr int rowHeight { 24 };

    /** @brief Palette width as a fraction of the parent width. */
    static constexpr float widthFraction { 0.6f };

    /** @brief Maximum list height as a fraction of the parent height. */
    static constexpr float maxListHeightFraction { 0.5f };

    /** @brief Background fill alpha applied over the window colour. */
    static constexpr float backgroundAlpha { 0.9f };

    /** @brief Horizontal padding inside each row column in pixels. */
    static constexpr int columnPadding { 8 };

    /** @brief Fraction of row width allocated to the name column. */
    static constexpr float nameColumnFraction { 0.25f };

    /** @brief Fraction of row width allocated to the description column. */
    static constexpr float descColumnFraction { 0.55f };

    // shortcut column takes the remaining fraction

    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActionList)
};

} // namespace Terminal
