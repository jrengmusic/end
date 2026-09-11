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
    auto* parentBar { dynamic_cast<jam::ButtonBar*> (bar.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getPosition() == map::Position::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, height));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (width, 0.0f));

        bounds = { 0.0f, 0.0f, height, width };
    }

    if (graphics.contains (Id::tabBar))
        jam::Svg::Flex::paint (g, *this, graphics.at (Id::tabBar), bounds);
}

void ENDLookAndFeel::drawBarHighlight (juce::Graphics& g, juce::Component& highlight)
{
    auto bounds { highlight.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::ButtonBar*> (highlight.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getPosition() == map::Position::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, height));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (width, 0.0f));

        bounds = { 0.0f, 0.0f, height, width };
    }

    if (graphics.contains (Id::tabHighlight))
        jam::Svg::Flex::paint (g, *this, graphics.at (Id::tabHighlight), bounds);
}

void ENDLookAndFeel::drawTabButton (juce::Graphics& g,
                                    juce::Button& button,
                                    bool isMouseOver,
                                    bool isMouseDown)
{
    auto bounds { button.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::ButtonBar*> (button.getParentComponent()) };
    const bool vertical { parentBar != nullptr and parentBar->isVertical() };

    if (vertical)
    {
        const auto width { bounds.getWidth() };
        const auto height { bounds.getHeight() };

        if (parentBar->getPosition() == map::Position::left)
            g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                .translated (0.0f, height));
        else
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi)
                                .translated (width, 0.0f));

        bounds = { 0.0f, 0.0f, height, width };
    }

    const auto state { jam::ButtonSVG::getState (
        button, isMouseOver, isMouseDown, map::ButtonState::getInstance()->get().size()) };
    const juce::Identifier stateId { map::ButtonState::getInstance()->get (state) };

    // Sparse bank — paint only when the state slot was authored in theme.md graphics section.
    if (graphics.contains (stateId))
        jam::Svg::Flex::paint (g, *this, graphics.at (stateId), bounds);
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
    auto fontFamily { config.getValue (Id::toType (Id::tab), Id::fontFamily) };
    auto fontSize { config.getValue (Id::toType (Id::tab), Id::textFontSize) };
    const float kerning { config.getValue (Id::toType (Id::tab), Id::kerningFactor) };

    return juce::FontOptions()
        .withName (fontFamily)
        .withPointHeight (fontSize)
        .withKerningFactor (kerning);
}

juce::Font ENDLookAndFeel::getCommonFont() const
{
    auto fontFamily { config.getValue (Id::toType (Id::tab), Id::fontFamily) };
    auto fontSize { config.getValue (Id::toType (Id::tab), Id::textFontSize) };

    return juce::FontOptions().withName (fontFamily).withPointHeight (fontSize);
}

juce::Font ENDLookAndFeel::getPopupMenuFont() { return getCommonFont(); }

int ENDLookAndFeel::getTabBarDepth (const jam::TabbedComponent& tabs) const noexcept
{
    const float depth { config.getValue (Id::toType (Id::tab), Id::depth) };
    const bool alwaysVisible { config.getValue (Id::toType (Id::tab), Id::alwaysVisible) };
    const bool shouldHide { tabs.getChildCount() <= 1 and not alwaysVisible };
    const int tabBarDepth { juce::roundToInt (getTabFont().getHeight() * depth) };

    return shouldHide ? 0 : tabBarDepth;
}

int ENDLookAndFeel::getTabPadding() const { return config.getValue (Id::toType (Id::tab), Id::textPadding); }
int ENDLookAndFeel::getTabPosition() const noexcept
{
    const juce::String position { config.getValue (Id::toType (Id::tab), Id::position) };
    return map::Position::getInstance()->get (position);
}

juce::String ENDLookAndFeel::getTabText (const juce::String& tabName) const
{
    if (bool uppercase { config.getValue (Id::toType (Id::tab), Id::uppercase) })
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
int ENDLookAndFeel::getPaneEdgeSize() const noexcept
{
    return config.getValue (Id::toType (Id::pane), Id::resizeBarThickness);
}

float ENDLookAndFeel::getPaneSidebarSize() const noexcept
{
    return config.getValue (Id::toType (Id::pane), Id::sidebarSize);
}

//==============================================================================
juce::Font ENDLookAndFeel::getCodeFont() const
{
    auto fontFamily { config.getValue (Id::toType (Id::code), Id::fontFamily) };
    auto fontSize { config.getValue (Id::toType (Id::code), Id::textFontSize) };

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

    const float cellWidthRatio { config.getValue (Id::toType (Id::code), Id::cellWidth) };
    const float lineHeightRatio { config.getValue (Id::toType (Id::code), Id::lineHeight) };

    const int cellWidth { juce::roundToInt (static_cast<float> (metrics.cellWidth)
                                            * cellWidthRatio) };
    const int cellHeight { juce::roundToInt (static_cast<float> (metrics.cellHeight)
                                             * lineHeightRatio) };

    return CodeMetrics { font, cellWidth, cellHeight, metrics.baseline };
}

juce::BorderSize<int> ENDLookAndFeel::getCodePadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    auto [top, right, bottom, left] = config.getInt16 (Id::toType (Id::code), Id::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

int ENDLookAndFeel::getGutterWidth() const noexcept
{
    return config.getValue (Id::toType (Id::scrollbar), Id::width);
}

bool ENDLookAndFeel::getCodeLigatures() const noexcept
{
    return config.getValue (Id::toType (Id::code), Id::ligatures);
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
    auto [top, right, bottom, left] = config.getInt16 (Id::toType (Id::tab), Id::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

int16_t ENDLookAndFeel::getWindowFX() const
{
    int16_t fx { 0 };

#if JUCE_MAC
    auto name { config.getValue (Id::toType (Id::style), Id::mac).toString() };
#elif JUCE_WINDOWS
    auto name { config.getValue (Id::toType (Id::style), Id::win).toString() };
#endif

    if (map::WindowFX::getInstance()->contains (name))
        fx = static_cast<int16_t> (map::WindowFX::getInstance()->get (name));

    return fx;
}

float ENDLookAndFeel::getWindowBlur() const noexcept
{
    return config.getValue (Id::toType (Id::window), Id::blurRadius);
}

void ENDLookAndFeel::prepareWindow (juce::Component& window)
{
    const auto colour { jam::ColourScheme::toColour (config.getValue (Id::toType (Id::window), Id::background)) };
    const auto blur { getWindowBlur() };
    const auto fx { getWindowFX() };
    const bool windowButtons { config.getValue (Id::toType (Id::display), Id::titleBarButtons) };

    jam::StyleWindow::apply (&window, colour);
    jam::BackgroundBlur::enable (&window,
                                 static_cast<jam::BackgroundBlur::WindowFX> (fx),
                                 blur,
                                 colour);

    if (auto* peer { window.getPeer() })
        jam::StyleWindow::setButtons (*peer, windowButtons);
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
                const auto fx { getWindowFX() };
                const float menuOpacity { config.getValue (Id::toType (Id::menu), Id::opacity) };
                const auto opacity { jam::BackgroundBlur::isEnabled() ? menuOpacity : 1.0f };
                const auto baseColour {
                    safeComponent->findColour (juce::PopupMenu::backgroundColourId).withAlpha (opacity)
                };
                const auto blur { jam::BackgroundBlur::isEnabled() ? getWindowBlur() : 0.0f };

                jam::StyleWindow::setMenu (safeComponent.getComponent(), baseColour);
                jam::BackgroundBlur::enable (safeComponent.getComponent(),
                                             static_cast<jam::BackgroundBlur::WindowFX> (fx),
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
    static const juce::String splitVertical { BinaryData::getString (files::splitVerticalNormal) };
    static const juce::String splitHorizontal { BinaryData::getString (files::splitHorizontalNormal) };
    static const juce::String joinCellsVertical { BinaryData::getString (files::joinCellsVerticalNormal) };
    static const juce::String joinCellsHorizontal { BinaryData::getString (files::joinCellsHorizontalNormal) };

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

static void drawMenuItemBackground (ENDLookAndFeel& laf,
                                    juce::Graphics& g,
                                    const juce::Rectangle<int>& area,
                                    juce::Rectangle<int>& r,
                                    bool isHighlighted,
                                    const juce::PopupMenu::Item& item)
{
    const auto* textColourToUse { item.colour != juce::Colour() ? &item.colour : nullptr };
    auto textColour { textColourToUse == nullptr ? laf.findColour (juce::PopupMenu::textColourId) : *textColourToUse };

    if (isHighlighted and item.isEnabled)
    {
        g.setColour (laf.findColour (juce::PopupMenu::highlightedBackgroundColourId));
        g.fillRect (r);

        g.setColour (laf.findColour (juce::PopupMenu::highlightedTextColourId));
    }
    else
    {
        g.setColour (textColour.withMultipliedAlpha (item.isEnabled ? 1.0f : 0.5f));

        if (item.isTicked)
        {
            g.setColour (laf.findColour (juce::PopupMenu::headerTextColourId));
        }
    }

    r.reduce (juce::jmin (5, area.getWidth() / 12), 0);
}

static void drawMenuItemIcon (ENDLookAndFeel& laf, juce::Graphics& g, juce::Rectangle<int>& r, const juce::PopupMenu::Item& item)
{
    auto maxFontHeight { static_cast<float> (r.getHeight()) };
    auto iconArea { r.removeFromLeft (juce::roundToInt (maxFontHeight)).toFloat() };

    const auto* svg { getMenuItemSVG (item.itemID) };

    if (svg != nullptr)
    {
        auto path { jam::Svg::getPath (svg, iconArea) };
        g.fillPath (path);
        r.removeFromLeft (juce::roundToInt (maxFontHeight * 0.5f));
    }
    else if (item.isTicked)
    {
        auto tick { laf.getTickShape (1.0f) };
        auto stroke { juce::PathStrokeType (2.0f) };
        auto delta { iconArea.getWidth() / 3 };
        g.strokePath (tick, stroke, tick.getTransformToScaleToFit (iconArea.reduced (delta).toFloat(), true));
    }
}

static void drawMenuItemArrow (ENDLookAndFeel& laf, juce::Graphics& g, juce::Rectangle<int>& r, const juce::PopupMenu::Item& item)
{
    const bool hasSubMenu { item.subMenu != nullptr and item.subMenu->getNumItems() > 0 };

    if (hasSubMenu)
    {
        auto arrowH { 0.6f * laf.getPopupMenuFont().getAscent() };

        auto x { static_cast<float> (r.removeFromRight (static_cast<int> (arrowH)).getX()) };
        auto halfH { static_cast<float> (r.getCentreY()) };

        juce::Path path;
        path.startNewSubPath (x, halfH - arrowH * 0.5f);
        path.lineTo (x + arrowH * 0.6f, halfH);
        path.lineTo (x, halfH + arrowH * 0.5f);

        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
}

static void drawMenuItemText (juce::Graphics& g, juce::Rectangle<int>& r, const juce::PopupMenu::Item& item, const juce::Font& font)
{
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
        auto r { area.reduced (1) };

        drawMenuItemBackground (*this, g, area, r, isHighlighted, item);

        auto font { getPopupMenuFont() };
        g.setFont (font);

        drawMenuItemIcon (*this, g, r, item);
        drawMenuItemArrow (*this, g, r, item);
        drawMenuItemText (g, r, item, font);
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

    if (graphics.contains (Id::resizerBar))
        jam::Svg::Flex::paint (g, *this, graphics.at (Id::resizerBar), bounds);

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
