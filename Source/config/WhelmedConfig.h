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
        inline static const juce::String fontStyle  { "font_style" };
        inline static const juce::String fontSize { "font_size" };
        inline static const juce::String codeFamily { "code_family" };
        inline static const juce::String codeSize   { "code_size" };
        inline static const juce::String codeStyle  { "code_style" };
        inline static const juce::String h1Size { "h1_size" };
        inline static const juce::String h2Size { "h2_size" };
        inline static const juce::String h3Size { "h3_size" };
        inline static const juce::String h4Size { "h4_size" };
        inline static const juce::String h5Size { "h5_size" };
        inline static const juce::String h6Size { "h6_size" };
        inline static const juce::String lineHeight { "line_height" };

        inline static const juce::String background          { "background" };
        inline static const juce::String bodyColour          { "body_colour" };
        inline static const juce::String codeColour          { "code_colour" };
        inline static const juce::String linkColour          { "link_colour" };
        inline static const juce::String h1Colour            { "h1_colour" };
        inline static const juce::String h2Colour            { "h2_colour" };
        inline static const juce::String h3Colour            { "h3_colour" };
        inline static const juce::String h4Colour            { "h4_colour" };
        inline static const juce::String h5Colour            { "h5_colour" };
        inline static const juce::String h6Colour            { "h6_colour" };
        inline static const juce::String codeFenceBackground { "code_fence_background" };
        inline static const juce::String progressBackground   { "progress_background" };
        inline static const juce::String progressForeground   { "progress_foreground" };
        inline static const juce::String progressTextColour   { "progress_text_colour" };
        inline static const juce::String progressSpinnerColour { "progress_spinner_colour" };

        inline static const juce::String paddingTop    { "padding_top" };
        inline static const juce::String paddingRight  { "padding_right" };
        inline static const juce::String paddingBottom { "padding_bottom" };
        inline static const juce::String paddingLeft   { "padding_left" };

        inline static const juce::String tokenError        { "token_error" };
        inline static const juce::String tokenComment      { "token_comment" };
        inline static const juce::String tokenKeyword      { "token_keyword" };
        inline static const juce::String tokenOperator     { "token_operator" };
        inline static const juce::String tokenIdentifier   { "token_identifier" };
        inline static const juce::String tokenInteger      { "token_integer" };
        inline static const juce::String tokenFloat        { "token_float" };
        inline static const juce::String tokenString       { "token_string" };
        inline static const juce::String tokenBracket      { "token_bracket" };
        inline static const juce::String tokenPunctuation  { "token_punctuation" };
        inline static const juce::String tokenPreprocessor { "token_preprocessor" };

        inline static const juce::String tableBackground       { "table_background" };
        inline static const juce::String tableHeaderBackground { "table_header_background" };
        inline static const juce::String tableRowAlt           { "table_row_alt" };
        inline static const juce::String tableBorderColour     { "table_border_colour" };
        inline static const juce::String tableHeaderText       { "table_header_text" };
        inline static const juce::String tableCellText         { "table_cell_text" };

        inline static const juce::String scrollbarThumb      { "scrollbar_thumb" };
        inline static const juce::String scrollbarTrack      { "scrollbar_track" };
        inline static const juce::String scrollbarBackground { "scrollbar_background" };

        inline static const juce::String scrollDown   { "scrollDown" };
        inline static const juce::String scrollUp     { "scrollUp" };
        inline static const juce::String scrollTop    { "scrollTop" };
        inline static const juce::String scrollBottom { "scrollBottom" };
        inline static const juce::String scrollStep   { "scrollStep" };

        inline static const juce::String loaderBackground    { "loader_background" };
        inline static const juce::String loaderFill          { "loader_fill" };
        inline static const juce::String loaderSpinnerColour { "loader_spinner_colour" };
        inline static const juce::String loaderTextColour    { "loader_text_colour" };
        inline static const juce::String loaderFontFamily    { "loader_font_family" };
        inline static const juce::String loaderFontSize      { "loader_font_size" };
        inline static const juce::String loaderFontStyle     { "loader_font_style" };
    };

    juce::String getString (const juce::String& key) const;
    float getFloat (const juce::String& key) const;
    int getInt (const juce::String& key) const;
    juce::Colour getColour (const juce::String& key) const;

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
