/**
 * @file Session.h
 * @brief One session's hosted-plugin registry — owns every plugin instance
 *        bound to the session's own panes.
 */
#pragma once
#include <JuceHeader.h>
#include "end/ENDModel.h"
#include "generated/Generated.h"

/**
 * @class Session
 * @brief Owns the hosted juce::AudioPluginInstance for each pane in one
 *        session, keyed by the pane's own identity.
 */
class Session
{
public:
    Session (jam::UUID newUuid, ENDModel& newModel);

    ~Session();

    /** @brief Resolves the hosted plugin instance bound to a pane.
     *  @param uuid Identity of the pane the plugin is bound to.
     *  @return The bound plugin instance.
     */
    juce::AudioPluginInstance& getPlugin (jam::UUID uuid);

    /** @brief Answers whether a pane has a bound plugin instance.
     *  @param uuid Identity of the pane.
     *  @return True when a plugin instance is bound to uuid.
     */
    bool contains (jam::UUID uuid) const;

    /** @brief Binds a plugin instance to a pane and stamps its name/identifier.
     *  @param uuid     Identity of the pane the plugin is bound to.
     *  @param pluginId Identifier of the plugin, or empty when instance is null.
     *  @param instance The plugin instance, or nullptr when creation failed.
     */
    void newPlugin (jam::UUID uuid, const juce::String& pluginId, std::unique_ptr<juce::AudioPluginInstance> instance);

    /** @brief Releases the plugin instance bound to a pane.
     *  @param uuid Identity of the pane whose plugin instance is released.
     */
    void removePlugin (jam::UUID uuid);

    juce::ValueTree state { Id::toType (Id::session) };

private:
    ENDModel& model;

    jam::HashMap<jam::UUID, std::unique_ptr<juce::AudioPluginInstance>> plugins;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Session)
};
