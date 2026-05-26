#pragma once

#include <JuceHeader.h>
#include "Parameter.h"
#include "TextBuffer.h"
#include "Identifier.h"
#include "../ModalType.h"
#include "../SelectionType.h"
#include "../lua/Engine.h"
#include "../AppState.h"
#include "../Map.h"
namespace terminal
{
/*____________________________________________________________________________*/

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
 *   id::SESSION   → AnyMap { root-level Parameter<int>s + Parameter<const char*>s }
 *   id::MODES     → AnyMap { mode Parameter<int>s }
 *   id::NORMAL    → AnyMap { per-screen Parameter<int>s for normal buffer }
 *   id::ALTERNATE → AnyMap { per-screen Parameter<int>s for alternate buffer }
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
struct State
    : public jam::ValueTree
    , private juce::ValueTree::Listener
{
    /**
     * @brief Constructs the State, walks Parameters.xml via buildLayout,
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
     * @param id       Property identifier (e.g. id::title).
     * @param rootNode The SESSION ValueTree node (direct property target).
     * @note MESSAGE THREAD — called from constructor only.
     */
    void addTextParameter (const juce::Identifier& id, juce::ValueTree& rootNode) noexcept;

    /** @brief Registers atomic mirrors for all int properties on a grafted node.
     *         Group key = node type. Called automatically on child addition. */
    void registerNodeAtomics (juce::ValueTree& node) noexcept;

    //==========================================================================
    // ValueTree access — MESSAGE THREAD only
    //==========================================================================

    bool getMode (const juce::Identifier& id) const noexcept;
    int getActiveScreen() const noexcept;

    cell getCols() const noexcept;
    cell getVisibleRows() const noexcept;

    juce::String getTitle() const noexcept;
    juce::String getCwd() const noexcept;
    juce::String getForegroundProcess() const noexcept;

    //==========================================================================
    // Reader-thread setters — lock-free, noexcept
    //==========================================================================

    void setId (const juce::String& uuid);

    void setScreen (int s) noexcept;
    void setMode (const juce::Identifier& id, bool value) noexcept;

    void setTitle (const char* src, int length) noexcept;
    void setCwd (const char* src, int length) noexcept;
    void setForegroundProcess (const char* src, int length) noexcept;

    /** @brief Builds the parameter schema from XML into this State's ValueTree and AnyMap.
     *  @param xml         Parsed AppParameters XML element.
     *  @param textBuffer  TextBuffer for TEXT parameter slot registration.
     */
    void buildLayout (const juce::XmlElement& xml, TextBuffer& textBuffer);

    // OSC 133 shell integration
    void setOutputBlockStart (cell row) noexcept;
    void setOutputBlockEnd (cell row) noexcept;
    void extendOutputBlock (cell row) noexcept;
    void setPromptRow (cell row) noexcept;
    cell getOutputBlockTop() const noexcept;
    cell getOutputBlockBottom() const noexcept;
    cell getPromptRow() const noexcept;
    bool hasOutputBlock() const noexcept;

    // Shell exit signal
    void setShellExited (bool exited) noexcept;
    bool getShellExited() const noexcept;

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

    // Cross-thread read/write — any thread, lock-free.
    void storeValue (const juce::Identifier& groupId, const juce::Identifier& paramId, int value) noexcept;
    int loadValue (const juce::Identifier& groupId, const juce::Identifier& paramId) const noexcept;

    // Flush
    bool refresh() noexcept;

private:
    static int resolveLayoutDefault (const juce::XmlElement& elem) noexcept;

    void valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child) override;

    void storeTextValue (const juce::Identifier& groupId, const juce::Identifier& paramId, const char* ptr) noexcept;

    TextBuffer& textBuffer;
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace terminal
