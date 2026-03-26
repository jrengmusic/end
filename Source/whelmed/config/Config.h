#pragma once

#include <JuceHeader.h>

namespace Whelmed
{

struct Config : jreng::Context<Config>
{
    Config();

    struct Value
    {
        enum class Type { string, number, boolean };
        Type expectedType;
        double minValue { 0.0 };
        double maxValue { 0.0 };
        bool hasRange { false };
    };

    struct Key
    {
        inline static const juce::String fontFamily { "font_family" };
        inline static const juce::String fontSize { "font_size" };
        inline static const juce::String codeFamily { "code_family" };
        inline static const juce::String codeSize { "code_size" };
        inline static const juce::String h1Size { "h1_size" };
        inline static const juce::String h2Size { "h2_size" };
        inline static const juce::String h3Size { "h3_size" };
        inline static const juce::String h4Size { "h4_size" };
        inline static const juce::String h5Size { "h5_size" };
        inline static const juce::String h6Size { "h6_size" };
        inline static const juce::String lineHeight { "line_height" };
    };

    juce::String getString (const juce::String& key) const;
    float getFloat (const juce::String& key) const;

    juce::String reload();
    const juce::String& getLoadError() const noexcept;

    std::function<void()> onReload;

private:
    void initKeys();
    bool load (const juce::File& file);
    bool load (const juce::File& file, juce::String& errorOut);
    void writeDefaults (const juce::File& file) const;
    juce::File getConfigFile() const;
    void addKey (const juce::String& key, const juce::var& defaultVal, Value spec);

    std::unordered_map<juce::String, juce::var> values;
    std::unordered_map<juce::String, Value> schema;
    juce::String loadError;
};

} // namespace Whelmed
