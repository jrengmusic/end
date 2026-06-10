#pragma once
#include <JuceHeader.h>
#include "../end/Map.h"

namespace config
{
/*____________________________________________________________________________*/

class Model
    : public jam::Model
    , public jam::Context<Model>
    , public jam::File::Watcher::Listener
{
public:
    //==========================================================================
    Model();

    ~Model() = default;

    juce::Rectangle<int> getInitWindowSize() const noexcept;

    /** @brief Reads each lua config file from disk and updates state.
        Sets ID::loadMessage property: "RELOAD" on success, error text on failure. */
    void loadFromPath();

private:
    void initialise();
    void saveToPath();
    void startWatching();
    /** @brief Reloads lua config on file update; dispatches property-change messages for SVG assets.
        @param file   The file that changed.
        @param event  The change event type. */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

    /** @brief Populates graphicsCallbacks from the live graphics node filenames.
        Must be called after loadFromPath() so runtime filenames are available. */
    void buildGraphicsCallbacks();

    /** @brief Watches ~/.config/end/ (lua) and the SVG asset subdirectory. */
    jam::File::Watcher watcher;

    /** @brief Type-based and Map-aware validators built during initialise() in a single walk.
     *  Outer key = tree type. Inner key = property name.
     *  String properties whose defaults match a known Map (Boolean, Position, GpuMode, DropMode)
     *  receive domain-constrained predicates; all others receive plain type predicates. */
    jam::lua::Validators validators;

    /** @brief SVG filename → sendPropertyChangeMessage callbacks, keyed by filename.
        Rebuilt by buildGraphicsCallbacks() on every loadFromPath(). */
    jam::Function::Map<juce::String, void> graphicsCallbacks;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Model)
};
/**______________________________END OF NAMESPACE______________________________*/
}// namespace config
