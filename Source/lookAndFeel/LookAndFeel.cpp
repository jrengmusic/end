#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

LookAndFeel::LookAndFeel()
{
    // registerTypeface() cannot run here — it needs the Vulkan glyph atlas,
    // which does not exist until end::Application constructs vulkanEngine
    // (this LookAndFeel constructs first, per end::Application's member order,
    // Main.height). Called once, externally, immediately after that construction.
    initialiseColours();
    loadGraphics();
    registerEvents();
    config.addListener (this);

    juce::LookAndFeel::setDefaultLookAndFeel (this);
}

LookAndFeel::~LookAndFeel() { config.removeListener (this); }

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

//==============================================================================
void LookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& bar)
{
    auto bounds { bar.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::button::Bar*> (bar.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getOrientation() == jam::button::Bar::Orientation::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, height));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (width, 0.0f));

        bounds = { 0.0f, 0.0f, height, width };
    }

    if (graphics.contains (ID::tabBar))
        jam::SVG::Flex::paint (g, *this, graphics.at (ID::tabBar), bounds);
}

void LookAndFeel::drawBarHighlight (juce::Graphics& g, juce::Component& highlight)
{
    auto bounds { highlight.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::button::Bar*> (highlight.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getOrientation() == jam::button::Bar::Orientation::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, height));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (width, 0.0f));

        bounds = { 0.0f, 0.0f, height, width };
    }

    if (graphics.contains (ID::tabHighlight))
        jam::SVG::Flex::paint (g, *this, graphics.at (ID::tabHighlight), bounds);
}

void LookAndFeel::drawTabButton (juce::Graphics& g,
                                 juce::Button& button,
                                 bool isMouseOver,
                                 bool isMouseDown)
{
    auto bounds { button.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::button::Bar*> (button.getParentComponent()) };
    const bool vertical { parentBar != nullptr and parentBar->isVertical() };

    if (vertical)
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getOrientation() == jam::button::Bar::Orientation::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, height));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (width, 0.0f));

        bounds = { 0.0f, 0.0f, height, width };
    }

    const auto state { jam::SVG::Button::getState (
        button, isMouseOver, isMouseDown, jam::map::ButtonState::get().size()) };
    const juce::Identifier stateId { jam::map::ButtonState::get (state) };

    // Sparse bank — paint only when the state slot was authored in theme.lua graphics section.
    if (graphics.contains (stateId))
        jam::SVG::Flex::paint (g, *this, graphics.at (stateId), bounds);
}

void LookAndFeel::drawTabLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));
    if (not label.isBeingEdited())
    {
        g.setFont (getTabFont());
        g.setColour (label.findColour (juce::Label::textColourId));
        g.drawText (
            getTabText (label.getText()), label.getLocalBounds(), juce::Justification::centred);
    }
}

//==============================================================================
juce::Font LookAndFeel::getTabFont() const
{
    auto fontFamily { config.getValue (IDtype::tab, ID::fontFamily) };
    auto fontSize { config.getValue (IDtype::tab, ID::fontSize) };
    const float kerning { config.getValue (IDtype::tab, ID::kerningFactor) };

    return juce::FontOptions()
        .withName (fontFamily)
        .withPointHeight (fontSize)
        .withKerningFactor (kerning);
}

int LookAndFeel::getTabBarDepth (const jam::TabbedComponent& tabs) const noexcept
{
    const float depth { config.getValue (IDtype::tab, ID::depth) };
    const bool alwaysVisible { config.getValue (IDtype::tab, ID::alwaysVisible) };
    const bool shouldHide { tabs.getNumTabs() <= 1 and not alwaysVisible };
    const int tabBarDepth { juce::roundToInt (getTabFont().getHeight() * depth) };

    return shouldHide ? 0 : tabBarDepth;
}

int LookAndFeel::getTabPadding() const { return config.getValue (IDtype::tab, ID::textPadding); }
int LookAndFeel::getTabPosition() const noexcept
{
    const juce::String position { config.getValue (IDtype::tab, ID::position) };
    return Position::get (position);
}

juce::String LookAndFeel::getTabText (const juce::String& tabName) const
{
    if (bool uppercase { config.getValue (IDtype::tab, ID::uppercase) })
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
int LookAndFeel::getPaneResizerBarSize() const noexcept
{
    return config.getValue (IDtype::pane, ID::resizeBarThickness);
}

//==============================================================================
juce::Font LookAndFeel::getCodeFont() const
{
    auto fontFamily { config.getValue (IDtype::code, ID::fontFamily) };
    auto fontSize { config.getValue (IDtype::code, ID::fontSize) };

    return juce::FontOptions().withName (fontFamily).withPointHeight (fontSize);
}

LookAndFeel::CodeMetrics LookAndFeel::getCodeMetrics (float zoom) const
{
    const auto baseFont { getCodeFont() };
    const juce::Font font { baseFont.withPointHeight (baseFont.getHeight() * zoom) };

    auto resolvedTypeface { font.getTypefacePtr() };

    // endless conformance restoration (jam::GlyphAtlas::calcMetrics(), commit
    // 2e37f6d) — cell metrics come from the FT face's own advance/ascender/
    // height at the exact size rasterize() sizes it to, rather than JUCE's
    // juce::GlyphArrangement::getStringWidth()/getAscent() estimate.
    auto* atlas { jam::GlyphAtlas::getInstance() };
    jassert (atlas != nullptr);
    const auto metrics { atlas->calcMetrics (resolvedTypeface, font.getHeight()) };

    const float cellWidthRatio { config.getValue (IDtype::code, ID::cellWidth) };
    const float lineHeightRatio { config.getValue (IDtype::code, ID::lineHeight) };

    const int cellWidth { juce::roundToInt (static_cast<float> (metrics.cellWidth) * cellWidthRatio) };
    const int cellHeight { juce::roundToInt (static_cast<float> (metrics.cellHeight) * lineHeightRatio) };

    return CodeMetrics { font, cellWidth, cellHeight, metrics.baseline };
}

juce::BorderSize<int> LookAndFeel::getCodePadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    auto [top, right, bottom, left] = config.getInt16 (IDtype::code, jam::ID::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

int LookAndFeel::getGutterWidth() const noexcept
{
    return config.getValue (IDtype::scrollbar, jam::ID::width);
}

LookAndFeel::CursorStyle LookAndFeel::getCursorStyle() const
{
    juce::String style { config.getValue (jam::IDtype::cursor, jam::ID::style).toString() };
    bool blink { config.getValue (jam::IDtype::cursor, ID::blink) };
    int blinkInterval { config.getValue (jam::IDtype::cursor, ID::blinkInterval) };
    juce::String cursorChar { config.getValue (jam::IDtype::cursor, ID::cursorChar).toString() };
    bool force { config.getValue (jam::IDtype::cursor, ID::force) };

    return CursorStyle { style, blink, blinkInterval, cursorChar, force };
}

bool LookAndFeel::getCodeLigatures() const noexcept
{
    return config.getValue (IDtype::code, ID::ligatures);
}

juce::String LookAndFeel::typefaceKey (const juce::String& name, const juce::String& style)
{
    return name + "/" + style;
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    auto name { font.getTypefaceName() };
    auto style { font.getTypefaceStyle() };
    auto key { typefaceKey (name, style) };

    if (typefaces.contains (key))
    {
        auto ptr { typefaces.at (key) };
        return ptr;
    }

    // "Book" is the regular weight of both embedded families — juce::Font
    // requests style "Regular" by default, which none of the six embedded
    // faces carry, so the exact key above misses on every unstyled Font.
    auto bookKey { typefaceKey (name, "Book") };

    if (typefaces.contains (bookKey))
    {
        auto ptr { typefaces.at (bookKey) };
        return ptr;
    }

    auto fallback { juce::LookAndFeel::getTypefaceForFont (font) };
    return fallback;
}

juce::BorderSize<int> LookAndFeel::getTabBarPadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    auto [top, right, bottom, left] = config.getInt16 (IDtype::tab, jam::ID::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

LookAndFeel::Style LookAndFeel::getWindowStyle() const
{
    auto colour { jam::Model::toColour (config.getValue (IDtype::window, jam::ID::background)) };
    int blur { config.getValue (IDtype::window, ID::blurRadius) };
    int16_t fx { 0 };

#if JUCE_MAC
    auto name { config.getValue (jam::IDtype::style, ID::mac).toString() };
#elif JUCE_WINDOWS
    auto name { config.getValue (jam::IDtype::style, ID::win).toString() };
#endif

    if (jam::map::WindowFX::getInstance()->contains (name))
        fx = static_cast<int16_t> (jam::map::WindowFX::get (name));

    const bool windowButtons { config.getValue (IDtype::display, ID::titleBarButtons) };

    return Style { colour, static_cast<int16_t> (blur), fx, windowButtons };
}

//==============================================================================
void LookAndFeel::drawResizerBar (juce::Graphics& g, juce::Component& bar)
{
    auto bounds { bar.getLocalBounds().toFloat() };
    auto* resizer { dynamic_cast<jam::PaneResizerBar*> (&bar) };

    if (resizer != nullptr and resizer->isVerticalBar())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                            .translated (0.0f, height));

        bounds = { 0.0f, 0.0f, height, width };
    }

    // Hover/pressed: swap bar colour to highlight
    const bool hover { bar.isMouseOver() or bar.isMouseButtonDown() };
    const auto savedColour { findColour (paneBarColourId) };

    if (hover)
        setColour (paneBarColourId, findColour (paneBarHighlightColourId));

    if (graphics.contains (ID::resizerBar))
        jam::SVG::Flex::paint (g, *this, graphics.at (ID::resizerBar), bounds);

    if (hover)
        setColour (paneBarColourId, savedColour);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
