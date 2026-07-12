#include "ENDLookAndFeel.h"

ENDLookAndFeel::ENDLookAndFeel()
{
    initialiseColours();
    loadGraphics();
    registerEvents();
    config.addListener (this);

    juce::LookAndFeel::setDefaultLookAndFeel (this);
}

ENDLookAndFeel::~ENDLookAndFeel() { config.removeListener (this); }

void ENDLookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree,
                                               const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

//==============================================================================
void ENDLookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& bar)
{
    auto bounds { bar.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::button::Bar*> (bar.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getPosition() == jam::Position::left)
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

void ENDLookAndFeel::drawBarHighlight (juce::Graphics& g, juce::Component& highlight)
{
    auto bounds { highlight.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::button::Bar*> (highlight.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getPosition() == jam::Position::left)
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

void ENDLookAndFeel::drawTabButton (juce::Graphics& g,
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

        if (parentBar->getPosition() == jam::Position::left)
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

void ENDLookAndFeel::drawTabLabel (juce::Graphics& g, juce::Label& label)
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
juce::Font ENDLookAndFeel::getTabFont() const
{
    auto fontFamily { config.getValue (IDtype::tab, ID::fontFamily) };
    auto fontSize { config.getValue (IDtype::tab, ID::fontSize) };
    const float kerning { config.getValue (IDtype::tab, ID::kerningFactor) };

    return juce::FontOptions()
        .withName (fontFamily)
        .withPointHeight (fontSize)
        .withKerningFactor (kerning);
}

juce::Font ENDLookAndFeel::getCommonFont() const
{
    auto fontFamily { config.getValue (IDtype::tab, ID::fontFamily) };
    auto fontSize { config.getValue (IDtype::tab, ID::fontSize) };

    return juce::FontOptions().withName (fontFamily).withPointHeight (fontSize);
}

juce::Font ENDLookAndFeel::getPopupMenuFont() { return getCommonFont(); }

int ENDLookAndFeel::getTabBarDepth (const jam::TabbedComponent& tabs) const noexcept
{
    const float depth { config.getValue (IDtype::tab, ID::depth) };
    const bool alwaysVisible { config.getValue (IDtype::tab, ID::alwaysVisible) };
    const bool shouldHide { tabs.getChildCount() <= 1 and not alwaysVisible };
    const int tabBarDepth { juce::roundToInt (getTabFont().getHeight() * depth) };

    return shouldHide ? 0 : tabBarDepth;
}

int ENDLookAndFeel::getTabPadding() const { return config.getValue (IDtype::tab, ID::textPadding); }
int ENDLookAndFeel::getTabPosition() const noexcept
{
    const juce::String position { config.getValue (IDtype::tab, ID::position) };
    return jam::Position::get (position);
}

juce::String ENDLookAndFeel::getTabText (const juce::String& tabName) const
{
    if (bool uppercase { config.getValue (IDtype::tab, ID::uppercase) })
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
int ENDLookAndFeel::getPaneEdgeSize() const noexcept
{
    return config.getValue (IDtype::pane, ID::resizeBarThickness);
}

float ENDLookAndFeel::getPaneSidebarSize() const noexcept
{
    return config.getValue (IDtype::pane, ID::sidebarSize);
}

//==============================================================================
juce::Font ENDLookAndFeel::getCodeFont() const
{
    auto fontFamily { config.getValue (IDtype::code, ID::fontFamily) };
    auto fontSize { config.getValue (IDtype::code, ID::fontSize) };

    return juce::FontOptions().withName (fontFamily).withPointHeight (fontSize);
}

ENDLookAndFeel::CodeMetrics ENDLookAndFeel::getCodeMetrics (float zoom) const
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

    const int cellWidth { juce::roundToInt (static_cast<float> (metrics.cellWidth)
                                            * cellWidthRatio) };
    const int cellHeight { juce::roundToInt (static_cast<float> (metrics.cellHeight)
                                             * lineHeightRatio) };

    return CodeMetrics { font, cellWidth, cellHeight, metrics.baseline };
}

juce::BorderSize<int> ENDLookAndFeel::getCodePadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    auto [top, right, bottom, left] = config.getInt16 (IDtype::code, jam::ID::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

int ENDLookAndFeel::getGutterWidth() const noexcept
{
    return config.getValue (IDtype::scrollbar, jam::ID::width);
}

ENDLookAndFeel::CursorStyle ENDLookAndFeel::getCursorStyle() const
{
    juce::String style { config.getValue (jam::IDtype::cursor, jam::ID::style).toString() };
    bool blink { config.getValue (jam::IDtype::cursor, ID::blink) };
    int blinkInterval { config.getValue (jam::IDtype::cursor, ID::blinkInterval) };
    juce::String cursorChar { config.getValue (jam::IDtype::cursor, ID::cursorChar).toString() };
    bool force { config.getValue (jam::IDtype::cursor, ID::force) };

    return CursorStyle { style, blink, blinkInterval, cursorChar, force };
}

bool ENDLookAndFeel::getCodeLigatures() const noexcept
{
    return config.getValue (IDtype::code, ID::ligatures);
}

juce::String ENDLookAndFeel::typefaceKey (const juce::String& name, const juce::String& style)
{
    return name + "/" + style;
}

juce::Typeface::Ptr ENDLookAndFeel::getTypefaceForFont (const juce::Font& font)
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

juce::BorderSize<int> ENDLookAndFeel::getTabBarPadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    auto [top, right, bottom, left] = config.getInt16 (IDtype::tab, jam::ID::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

ENDLookAndFeel::Style ENDLookAndFeel::getWindowStyle() const
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

void ENDLookAndFeel::preparePopupMenuWindow (juce::Component& newWindow)
{
    newWindow.setOpaque (false);

    auto safeComponent { juce::Component::SafePointer<juce::Component> (&newWindow) };

    juce::MessageManager::callAsync (
        [this, safeComponent]
        {
            if (safeComponent != nullptr)
            {
                const auto windowStyle { getWindowStyle() };
                const float menuOpacity { config.getValue (IDtype::menu, ID::opacity) };
                const auto opacity { jam::BackgroundBlur::isEnabled() ? menuOpacity : 1.0f };
                const auto baseColour {
                    safeComponent->findColour (juce::PopupMenu::backgroundColourId).withAlpha (opacity)
                };
                const auto blur { jam::BackgroundBlur::isEnabled()
                                      ? static_cast<float> (windowStyle.blur)
                                      : 0.0f };

                jam::style::window::setMenu (safeComponent.getComponent(), baseColour);
                jam::BackgroundBlur::enable (safeComponent.getComponent(),
                                             static_cast<jam::BackgroundBlur::WindowFX> (windowStyle.fx),
                                             blur,
                                             baseColour);
            }
        });
}

void ENDLookAndFeel::drawPopupMenuBackgroundWithOptions (juce::Graphics& g,
                                                          int width,
                                                          int height,
                                                          const juce::PopupMenu::Options&)
{
#if JUCE_WINDOWS
    if (not jam::BackgroundBlur::isEnabled())
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
#else
    juce::ignoreUnused (g, width, height);
#endif
}

static const char* getMenuItemSVG (int itemID)
{
    static const juce::String splitVertical { BinaryData::getString ("split_vertical_normal.svg") };
    static const juce::String splitHorizontal { BinaryData::getString ("split_horizontal_normal.svg") };
    static const juce::String joinCellsVertical { BinaryData::getString ("join_cells_vertical_normal.svg") };
    static const juce::String joinCellsHorizontal { BinaryData::getString ("join_cells_horizontal_normal.svg") };

    switch (itemID)
    {
        case 1: return splitVertical.toRawUTF8();
        case 2: return splitHorizontal.toRawUTF8();
        case 3:
        case 4: return joinCellsVertical.toRawUTF8();
        case 5:
        case 6: return joinCellsHorizontal.toRawUTF8();
        default: return nullptr;
    }
}

void ENDLookAndFeel::drawPopupMenuItemWithOptions (juce::Graphics& g,
                                                   const juce::Rectangle<int>& area,
                                                   bool isHighlighted,
                                                   const juce::PopupMenu::Item& item,
                                                   const juce::PopupMenu::Options&)
{
    if (item.isSeparator)
    {
        auto r { area.reduced (5, 0) };
        r.removeFromTop (juce::roundToInt ((static_cast<float> (r.getHeight()) * 0.5f) - 0.5f));

        g.setColour (findColour (juce::PopupMenu::textColourId).withAlpha (0.3f));
        g.fillRect (r.removeFromTop (1));
    }
    else
    {
        const auto* textColourToUse { item.colour != juce::Colour() ? &item.colour : nullptr };
        auto textColour { textColourToUse == nullptr ? findColour (juce::PopupMenu::textColourId) : *textColourToUse };

        auto r { area.reduced (1) };

        if (isHighlighted and item.isEnabled)
        {
            g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect (r);

            g.setColour (findColour (juce::PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour (textColour.withMultipliedAlpha (item.isEnabled ? 1.0f : 0.5f));

            if (item.isTicked)
            {
                g.setColour (findColour (juce::PopupMenu::headerTextColourId));
            }
        }

        r.reduce (juce::jmin (5, area.getWidth() / 12), 0);

        auto font { getPopupMenuFont() };

        auto maxFontHeight { static_cast<float> (r.getHeight()) };

        g.setFont (font);

        auto iconArea { r.removeFromLeft (juce::roundToInt (maxFontHeight)).toFloat() };

        const auto* svg { getMenuItemSVG (item.itemID) };

        if (svg != nullptr)
        {
            auto path { jam::SVG::getPath (svg, iconArea) };
            g.fillPath (path);
            r.removeFromLeft (juce::roundToInt (maxFontHeight * 0.5f));
        }
        else if (item.isTicked)
        {
            auto tick { getTickShape (1.0f) };
            auto stroke { juce::PathStrokeType (2.0f) };
            auto delta { iconArea.getWidth() / 3 };
            g.strokePath (tick, stroke, tick.getTransformToScaleToFit (iconArea.reduced (delta).toFloat(), true));
        }

        const bool hasSubMenu { item.subMenu != nullptr and item.subMenu->getNumItems() > 0 };

        if (hasSubMenu)
        {
            auto arrowH { 0.6f * getPopupMenuFont().getAscent() };

            auto x { static_cast<float> (r.removeFromRight (static_cast<int> (arrowH)).getX()) };
            auto halfH { static_cast<float> (r.getCentreY()) };

            juce::Path path;
            path.startNewSubPath (x, halfH - arrowH * 0.5f);
            path.lineTo (x + arrowH * 0.6f, halfH);
            path.lineTo (x, halfH + arrowH * 0.5f);

            g.strokePath (path, juce::PathStrokeType (2.0f));
        }

        r.removeFromRight (3);
        g.drawFittedText (item.text, r, juce::Justification::centredLeft, 1);

        if (item.shortcutKeyDescription.isNotEmpty())
        {
            auto f2 { font.withPointHeight (font.getHeightInPoints() * 0.75f) };
            f2.setHorizontalScale (0.95f);
            g.setFont (f2);

            g.drawText (item.shortcutKeyDescription, r, juce::Justification::centredRight, true);
        }
    }
}

//==============================================================================
void ENDLookAndFeel::drawPaneEdge (juce::Graphics& g, juce::Component& bar)
{
    auto& paneEdge { static_cast<jam::PaneEdge&> (bar) };
    auto bounds { paneEdge.getSeam().toFloat() };

    if (bounds.getWidth() < bounds.getHeight())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                            .translated (0.0f, height)
                            .translated (bounds.getX(), bounds.getY()));

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

void ENDLookAndFeel::drawPaneOutline (juce::Graphics& g, juce::Component& pane)
{
    auto colour { findColour (pane.hasKeyboardFocus (true)
                                  ? jam::PaneComponent::focusedOutlineColourId
                                  : jam::PaneComponent::outlineColourId) };

    g.setColour (colour);
    g.drawRoundedRectangle (
        pane.getLocalBounds().toFloat().reduced (jam::PaneComponent::edgePadding
                                                 + jam::PaneComponent::lineThickness),
        jam::PaneComponent::cornerSize,
        jam::PaneComponent::lineThickness);
}
