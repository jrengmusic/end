#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

LookAndFeel::LookAndFeel()
{
    // registerTypeface() cannot run here — it needs the Vulkan glyph atlas,
    // which does not exist until end::Application constructs vulkanEngine
    // (this LookAndFeel constructs first, per end::Application's member order,
    // Main.h). Called once, externally, immediately after that construction.
    initialiseColours();
    loadGraphics();
    registerEvents();
    config.addListener (this);
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
        const auto w { bounds.getWidth() };
        const auto h { bounds.getHeight() };

        if (parentBar->getOrientation() == jam::button::Bar::Orientation::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, h));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (w, 0.0f));

        bounds = { 0.0f, 0.0f, h, w };
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
        const auto w { bounds.getWidth() };
        const auto h { bounds.getHeight() };

        if (parentBar->getOrientation() == jam::button::Bar::Orientation::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, h));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (w, 0.0f));

        bounds = { 0.0f, 0.0f, h, w };
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
        const auto w { bounds.getWidth() };
        const auto h { bounds.getHeight() };

        if (parentBar->getOrientation() == jam::button::Bar::Orientation::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, h));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (w, 0.0f));

        bounds = { 0.0f, 0.0f, h, w };
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

juce::Font LookAndFeel::getTabFont() const
{
    auto fontFamily { config.getValue (IDtype::tab, ID::fontFamily) };
    auto fontSize { config.getValue (IDtype::tab, ID::fontSize) };
    const float kerning { config.getValue (IDtype::tab, ID::kerningFactor) };

    auto font { juce::Font { juce::FontOptions()
                            .withName (fontFamily)
                            .withPointHeight (fontSize)
                            .withKerningFactor (kerning) } };

    return font;
}

int LookAndFeel::getTabPadding() const { return config.getValue (IDtype::tab, ID::textPadding); }

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

juce::String LookAndFeel::getTabText (const juce::String& tabName) const
{
    if (bool uppercase { config.getValue (IDtype::tab, ID::uppercase) })
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
void LookAndFeel::drawResizerBar (juce::Graphics& g, juce::Component& bar)
{
    auto bounds { bar.getLocalBounds().toFloat() };
    auto* resizer { dynamic_cast<jam::PaneResizerBar*> (&bar) };

    if (resizer != nullptr and resizer->isVerticalBar())
    {
        const auto w { bounds.getWidth() };
        const auto h { bounds.getHeight() };

        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                            .translated (0.0f, h));

        bounds = { 0.0f, 0.0f, h, w };
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
