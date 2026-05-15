#pragma once

#include <JuceHeader.h>
#include "Parameter.h"
#include "TextBuffer.h"
#include "Identifier.h"
#include "../../ModalType.h"
#include "../../SelectionType.h"
#include "../../lua/Engine.h"
#include "../../AppState.h"

namespace Terminal
{

using ::ModalType;
using ::SelectionType;

/**
 * @struct State
 * @brief XML-driven terminal parameter store: reader thread writes parameters,
 *        timer flushes to ValueTree, UI reads ValueTree.
 *
 * ### Architecture
 * Mirrors JUCE AudioProcessorValueTreeState (APVTS) 1:1:
 * - `Parameters.xml` declares the schema (ParameterLayout equivalent).
 * - `Parameter<int>` / `Parameter<const char*>` are the adapters (ParameterAdapter equivalent).
 * - One `jam::AnyMap params` (nested for hierarchy) is the adapter table.
 * - One `juce::ValueTree state` is the SSOT for the UI.
 * - `flush()` is the one loop — each Parameter writes itself (no-arg flush).
 *
 * ### Map structure
 * ```
 * params {
 *   ID::SESSION   → AnyMap { root-level Parameter<int>s + Parameter<const char*>s }
 *   ID::MODES     → AnyMap { mode Parameter<int>s }
 *   ID::NORMAL    → AnyMap { per-screen Parameter<int>s for normal buffer }
 *   ID::ALTERNATE → AnyMap { per-screen Parameter<int>s for alternate buffer }
 * }
 * ```
 *
 * ### ValueTree structure
 * ```
 * SESSION
 * ├── PARAM id="activeScreen" value=…
 * ├── … (all root-level params)
 * ├── MODES
 * │   ├── PARAM id="applicationCursor" value=…
 * │   └── …
 * ├── NORMAL
 * │   ├── PARAM id="cursorRow" value=…
 * │   └── …
 * └── ALTERNATE
 *     └── …
 * ```
 *
 * ### Thread ownership
 * - `set*()` methods: any thread, lock-free, noexcept.
 * - `get*()` atomic getters: any thread, lock-free, noexcept.
 * - `get*()` ValueTree getters: MESSAGE THREAD only.
 * - `timerCallback()` / `flush()`: MESSAGE THREAD only.
 */
struct State : public jam::ValueTree
{
    /**
     * @brief Constructs the State, walks Parameters.xml via Layout::build,
     *        populates the parameter map, and starts the flush timer at 60 Hz.
     * @param textBuffer  Session-owned double-buffered string storage.
     * @note MESSAGE THREAD.
     */
    explicit State (TextBuffer& textBuffer);

    /** @note MESSAGE THREAD. */
    ~State() override;

    //==========================================================================
    // SSOT registration — domain-specific additions
    //==========================================================================

    /**
     * @brief Creates one Parameter<const char*> for a TEXT parameter.
     *
     * The parameter flushes to a direct property on rootNode (not a PARAM child).
     *
     * @param id       Property identifier (e.g. ID::title).
     * @param rootNode The SESSION ValueTree node (direct property target).
     * @note MESSAGE THREAD — called from constructor only.
     */
    void addTextParameter (const juce::Identifier& id,
                           juce::ValueTree& rootNode) noexcept;

    //==========================================================================
    // ValueTree access — MESSAGE THREAD only
    //==========================================================================

    bool getMode (const juce::Identifier& id) const noexcept;
    int getActiveScreen() const noexcept;
    uint32_t getKeyboardFlags() const noexcept;

    int getCursorRow() const noexcept;
    int getCursorCol() const noexcept;
    bool isCursorVisible() const noexcept;
    int getCursorShape() const noexcept;
    int getCursorColor() const noexcept;
    int getCols() const noexcept;
    int getVisibleRows() const noexcept;
    int getCellWidth() const noexcept;
    int getCellHeight() const noexcept;
    int getBaseline() const noexcept;
    float getFontSize() const noexcept;

    juce::String getTitle() const noexcept;
    juce::String getCwd() const noexcept;
    juce::String getForegroundProcess() const noexcept;

    //==========================================================================
    // Reader-thread setters — lock-free, noexcept
    //==========================================================================

    void setId (const juce::String& uuid);

    void setScreen (int s) noexcept;
    void setMode (const juce::Identifier& id, bool value) noexcept;

    void setCursorRow (int s, int row) noexcept;
    void setCursorCol (int s, int col) noexcept;
    void setCursorVisible (int s, bool v) noexcept;
    void setCursorShape (int s, int shape) noexcept;
    void setCursorColor (int s, juce::Colour colour) noexcept;
    void resetCursorColor (int s) noexcept;

    void setTitle (const char* src, int length) noexcept;
    void setCwd (const char* src, int length) noexcept;
    void setForegroundProcess (const char* src, int length) noexcept;

    void setDimensions (int cols, int rows) noexcept;
    void setCellMetrics (int cellWidth, int cellHeight, int baseline, float fontSize) noexcept;

    // Keyboard mode stack
    void pushKeyboardMode (int s, uint32_t flags) noexcept;
    void popKeyboardMode (int s, int count) noexcept;
    void setKeyboardMode (int s, uint32_t flags, int mode) noexcept;
    void resetKeyboardMode (int s) noexcept;

    // OSC 133 shell integration
    void setOutputBlockStart (int row) noexcept;
    void setOutputBlockEnd (int row) noexcept;
    void extendOutputBlock (int row) noexcept;
    void setPromptRow (int row) noexcept;
    int getOutputBlockTop() const noexcept;
    int getOutputBlockBottom() const noexcept;
    int getPromptRow() const noexcept;
    bool hasOutputBlock() const noexcept;

    // Snapshot signal
    void setSnapshotDirty() noexcept;
    bool consumeSnapshotDirty() noexcept;
    bool isSnapshotDirty() const noexcept;

    // Paste echo gate
    void setPasteEchoGate (int bytes) noexcept;
    void consumePasteEcho (int bytes) noexcept;
    void clearPasteEchoGate() noexcept;

    // Sync output (mode 2026)
    void setSyncOutput (bool active) noexcept;
    bool isSyncOutputActive() const noexcept;
    void requestSyncResize() noexcept;
    bool consumeSyncResize() noexcept;

    // Selection
    void setSelectionType (int type) noexcept;
    int getSelectionType() const noexcept;
    void setSelectionCursor (int row, int col) noexcept;
    int getSelectionCursorRow() const noexcept;
    int getSelectionCursorCol() const noexcept;
    void setSelectionAnchor (int row, int col) noexcept;
    int getSelectionAnchorRow() const noexcept;
    int getSelectionAnchorCol() const noexcept;
    void setDragAnchor (int row, int col) noexcept;
    int getDragAnchorRow() const noexcept;
    int getDragAnchorCol() const noexcept;
    void setDragActive (bool active) noexcept;
    bool isDragActive() const noexcept;

    // Preview split-viewport
    void dismissPreview() noexcept;
    bool isPreviewActive() const noexcept;
    int getSplitCol() const noexcept;

    // Hints
    void setHintPage (int page) noexcept;
    int getHintPage() const noexcept;
    void setHintTotalPages (int total) noexcept;
    int getHintTotalPages() const noexcept;

    // Modal
    void setModalType (ModalType type) noexcept;
    ModalType getModalType() const noexcept;
    bool isModal() const noexcept;

    // Scroll-off counter — reader thread increments, message thread consumes.
    void addScrolledRows (int count) noexcept;
    int consumeScrolledRows() noexcept;
    int getScrolledRows() const noexcept;

    // Scrollback stub (live callers, scrollback state removed)
    int getScrollbackUsed() const noexcept;

    // Per-screen atomic loaders — any thread, lock-free.
    // Used by Processor's ID::screenSwitch handler to read the new screen's saved cursor.
    int      loadCursorRow     (int s) const noexcept;
    int      loadCursorCol     (int s) const noexcept;
    bool     loadCursorVisible (int s) const noexcept;
    uint32_t loadKeyboardFlags (int s) const noexcept;

    // Flush
    bool refresh() noexcept;

private:
    void storeValue (const juce::Identifier& groupId,
                     const juce::Identifier& paramId,
                     int value) noexcept;

    int loadValue (const juce::Identifier& groupId,
                   const juce::Identifier& paramId) const noexcept;

    void storeFloatValue (const juce::Identifier& groupId,
                          const juce::Identifier& paramId,
                          float value) noexcept;

    void storeTextValue (const juce::Identifier& groupId,
                         const juce::Identifier& paramId,
                         const char* ptr) noexcept;

    TextBuffer& textBuffer;

    std::atomic<int> scrolledRows { 0 };  ///< Scroll-off rows pending drain. Reader writes, message thread consumes.

    // Keyboard mode stack
    static constexpr int maxKeyboardStackDepth { 16 };
    juce::HeapBlock<uint32_t> keyboardModeStack;
    juce::HeapBlock<int> keyboardModeStackSize;
};

} // namespace Terminal
