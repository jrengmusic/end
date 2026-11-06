/**
 * @file PluginState.h
 * @brief Raw state bridge of one hosted plugin instance: it writes the terminal rows of a pane into the plugin
 *        and reads them back when the plugin reports a change.
 */
#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"

/**
 * @class PluginState
 * @brief Holds the terminal rows of one pane, and synchronises them with the raw state of its hosted plugin instance.
 *
 * The constructor adds the object as a listener of the instance, and the destructor removes it.
 * The raw state read and write function is selected by the plugin format name of the instance, in the format tables of the source file.
 */
class PluginState : private juce::AudioProcessorListener
{
public:
    /**
     * @brief Adds the object as a listener of the instance, and takes the `terminal` child of the pane row.
     *
     * The `terminal` child is created in the pane row when it is absent.
     *
     * @param newInstance  Plugin instance whose raw state the object reads and writes. The graph owns the instance.
     * @param paneRow      Pane row that holds the `terminal` child.
     */
    PluginState (juce::AudioPluginInstance& newInstance, juce::ValueTree paneRow);

    /** @brief Removes the object from the listeners of the instance. */
    ~PluginState() override;

    /**
     * @brief Writes a `terminal` tree to the raw state of the instance.
     *
     * The tree is serialised to bytes, then the write function of the plugin format applies the bytes to the instance.
     *
     * @param rows  Tree whose properties are the terminal rows to write.
     */
    void setRows (const juce::ValueTree& rows);

private:
    /** @brief Does nothing. The object reads the state only on a change of the processor. */
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override;

    /** @brief Reads the raw state of the instance, then copies the properties of its `terminal` child to the `terminal` child of the pane row. */
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override;

    /** @brief Plugin instance whose raw state the object reads and writes. The graph owns it, and it outlives the object. */
    juce::AudioPluginInstance& instance;

    /** @brief The `terminal` child of the pane row. The object writes the read properties to it. */
    juce::ValueTree terminal;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginState)
};
