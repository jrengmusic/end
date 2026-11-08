#include "ENDLookAndFeel.h"

static bool isPaneFocused (juce::Component& pane)
{
    jassert (dynamic_cast<jam::OwnedComponent*> (&pane) != nullptr);

    return static_cast<bool> (static_cast<jam::OwnedComponent&> (pane).getValueTree().getProperty (Id::focus));
}

static juce::Rectangle<float> getPaneArea (const juce::Component& pane, int padding)
{
    return pane.getLocalBounds().toFloat().reduced (static_cast<float> (padding) + jam::PaneComponent::lineThickness);
}

static juce::Rectangle<float> applyRotation (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::AffineTransform& transform)
{
    g.addTransform (transform);

    return { 0.0f, 0.0f, bounds.getHeight(), bounds.getWidth() };
}

static juce::FontOptions getTabFontOptions (ConfigModel& config)
{
    auto fontFamily { config.getRowValue (jam::Format::toType (Id::tab), Id::textFontFamily) };
    auto fontSize { config.getRowValue (jam::Format::toType (Id::tab), Id::textFontSize) };

    return juce::FontOptions().withName (fontFamily).withPointHeight (fontSize);
}

ENDLookAndFeel::ENDLookAndFeel()
{
    applyColours();
    loadGraphics();
    registerEvents();
    config.addListener (this);
    juce::Desktop::getInstance().addDarkModeSettingListener (this);

    juce::LookAndFeel::setDefaultLookAndFeel (this);
}

ENDLookAndFeel::~ENDLookAndFeel()
{
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
    config.removeListener (this);
}

void ENDLookAndFeel::darkModeSettingChanged()
{
    const juce::String appearanceName { config.getRowValue (jam::Format::toType (Id::display), Id::appearance) };

    if (map::Appearance::getInstance()->get (appearanceName) == map::Appearance::automatic)
        applyColours();
}

void ENDLookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree,
                                               const juce::Identifier& property)
{
    const auto key { ConfigModel::getEventKey (events, tree, property) };

    if (events.contains (key))
        events.get (key, tree);
}

//==============================================================================
juce::AffineTransform ENDLookAndFeel::getVerticalTextTransform (int position,
                                                                 float width,
                                                                 float height) const noexcept
{
    return position == map::Position::left
        ? juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi).translated (0.0f, height)
        : juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi).translated (width, 0.0f);
}

juce::Rectangle<float> ENDLookAndFeel::applyBarRotation (juce::Graphics& g, juce::Component& component) const
{
    auto bounds { component.getLocalBounds().toFloat() };
    auto* parentBar { dynamic_cast<jam::ButtonBar*> (component.getParentComponent()) };

    if (parentBar != nullptr and parentBar->isVertical())
        bounds = applyRotation (g, bounds, getVerticalTextTransform (parentBar->getPosition(), bounds.getWidth(), bounds.getHeight()));

    return bounds;
}

void ENDLookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& bar)
{
    jam::Svg::Flex::paint (g, *this, graphics.at (Id::tabBar), applyBarRotation (g, bar));
}

void ENDLookAndFeel::drawBarHighlight (juce::Graphics& g, juce::Component& highlight)
{
    jam::Svg::Flex::paint (g, *this, graphics.at (Id::tabHighlight), applyBarRotation (g, highlight));
}

void ENDLookAndFeel::drawTabButton (juce::Graphics& g,
                                    juce::Button& button,
                                    bool isMouseOver,
                                    bool isMouseDown)
{
    const auto bounds { applyBarRotation (g, button) };
    const auto state { jam::ButtonSVG::getState (button, isMouseOver, isMouseDown) };
    const juce::Identifier stateId { map::ButtonState::getInstance()->get (state) };

    if (graphics.contains (stateId))
        jam::Svg::Flex::paint (g, *this, graphics.at (stateId), bounds);
}

void ENDLookAndFeel::drawTabLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (not label.isBeingEdited())
    {
        g.setFont (juce::Font { getTabFont() });
        g.setColour (label.findColour (juce::Label::textColourId));
        g.drawText (
            getTabText (label.getText()), label.getLocalBounds(), juce::Justification::centred);
    }
}

//==============================================================================
juce::FontOptions ENDLookAndFeel::getTabFont() const
{
    const float kerning { config.getRowValue (jam::Format::toType (Id::tab), Id::kerningFactor) };

    return getTabFontOptions (config).withKerningFactor (kerning);
}

juce::FontOptions ENDLookAndFeel::getOverlayFont() const
{
    return juce::FontOptions()
        .withName (config.getRowValue (jam::Format::toType (Id::overlay), Id::textFontFamily).toString())
        .withPointHeight (static_cast<float> (config.getRowValue (jam::Format::toType (Id::overlay), Id::textFontSize)));
}

juce::Font ENDLookAndFeel::getPopupMenuFont() { return getTabFontOptions (config); }

int ENDLookAndFeel::getTabBarDepth (const jam::TabbedComponent& tabs) const noexcept
{
    const float depth { config.getRowValue (jam::Format::toType (Id::tab), Id::depth) };
    const bool alwaysVisible { config.getRowValue (jam::Format::toType (Id::tab), Id::alwaysVisible) };
    const bool shouldHide { tabs.getNumChildren() <= 1 and not alwaysVisible };
    const int tabBarDepth { juce::roundToInt (juce::Font { getTabFont() }.getHeight() * depth) };

    return shouldHide ? 0 : tabBarDepth;
}

int ENDLookAndFeel::getTabPadding() const { return config.getRowValue (jam::Format::toType (Id::tab), Id::textPadding); }

int ENDLookAndFeel::getTabPosition() const noexcept
{
    const juce::String position { config.getRowValue (jam::Format::toType (Id::tab), Id::position) };
    return map::Position::getInstance()->get (position);
}

juce::String ENDLookAndFeel::getTabText (const juce::String& tabName) const
{
    if (bool uppercase { config.getRowValue (jam::Format::toType (Id::tab), Id::uppercase) })
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
int ENDLookAndFeel::getPaneEdgeSize() const noexcept
{
    return config.getRowValue (jam::Format::toType (Id::pane), Id::resizeBarThickness);
}

int ENDLookAndFeel::getPanePadding() const noexcept
{
    return config.getRowValue (jam::Format::toType (Id::pane), Id::padding);
}

//==============================================================================
juce::String ENDLookAndFeel::getTypefaceKey (const juce::String& name, const juce::String& style)
{
    static constexpr const char* styleSeparator { "/" };

    return name + styleSeparator + style;
}

juce::Typeface::Ptr ENDLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    auto name { font.getTypefaceName() };
    auto style { font.getTypefaceStyle() };
    auto key { getTypefaceKey (name, style) };

    if (typefaces.contains (key))
        return typefaces.at (key);

    // "Book" is the regular weight of both embedded families — juce::Font
    // requests style "Regular" by default, which none of the six embedded
    // faces carry, so the exact key above misses on every unstyled Font.
    static constexpr const char* regularStyle { "Book" };
    auto bookKey { getTypefaceKey (name, regularStyle) };

    if (typefaces.contains (bookKey))
        return typefaces.at (bookKey);

    return juce::LookAndFeel::getTypefaceForFont (font);
}

juce::BorderSize<int> ENDLookAndFeel::getTabBarPadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    const auto padding { config.getRowValue (jam::Format::toType (Id::tab), Id::padding) };

    constexpr int top { 0 };
    constexpr int right { 1 };
    constexpr int bottom { 2 };
    constexpr int left { 3 };

    return juce::BorderSize<int> { padding[top], padding[left], padding[bottom], padding[right] };
}

int16_t ENDLookAndFeel::getWindowFX() const
{
    int16_t fx { 0 };

#if JUCE_MAC
    auto name { config.getRowValue (jam::Format::toType (Id::style), Id::mac).toString() };
#elif JUCE_WINDOWS
    auto name { config.getRowValue (jam::Format::toType (Id::style), Id::win).toString() };
#else
    juce::String name;
#endif

    if (map::WindowFX::getInstance()->contains (name))
        fx = static_cast<int16_t> (map::WindowFX::getInstance()->get (name));

    return fx;
}

float ENDLookAndFeel::getWindowBlur() const noexcept
{
    return config.getRowValue (jam::Format::toType (Id::window), Id::blurRadius);
}

void ENDLookAndFeel::prepareWindow (juce::Component& window)
{
    const auto colour { jam::ColourScheme::toColour (config.getRowValue (jam::Format::toType (Id::window), Id::background)) };
    const auto blur { getWindowBlur() };
    const auto fx { getWindowFX() };

    auto* jamWindow { dynamic_cast<jam::Window*> (&window) };
    jassert (jamWindow != nullptr);

    jamWindow->setStyle (colour, blur, static_cast<jam::BackgroundBlur::WindowFX> (fx));
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
                const float menuOpacity { config.getRowValue (jam::Format::toType (Id::menu), Id::opacity) };
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
    static const jam::HashMap<int, juce::String> menuItemIcons {
        { 1, BinaryData::getString (files::splitVerticalNormal) },
        { 2, BinaryData::getString (files::splitHorizontalNormal) },
        { 3, BinaryData::getString (files::joinCellsVerticalNormal) },
        { 4, BinaryData::getString (files::joinCellsVerticalNormal) },
        { 5, BinaryData::getString (files::joinCellsHorizontalNormal) },
        { 6, BinaryData::getString (files::joinCellsHorizontalNormal) }
    };

    if (menuItemIcons.contains (itemID)) return menuItemIcons.at (itemID).toRawUTF8();
    return nullptr;
}

static void drawMenuItemBackground (ENDLookAndFeel& laf,
                                     juce::Graphics& g,
                                     const juce::Rectangle<int>& area,
                                     juce::Rectangle<int>& r,
                                     bool isHighlighted,
                                     const juce::PopupMenu::Item& item)
{
    constexpr float disabledTextAlpha { 0.5f };
    constexpr int maxHorizontalReduce { 5 };
    constexpr int areaWidthReduceDivisor { 12 };

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
        g.setColour (textColour.withMultipliedAlpha (item.isEnabled ? 1.0f : disabledTextAlpha));

        if (item.isTicked)
        {
            g.setColour (laf.findColour (juce::PopupMenu::headerTextColourId));
        }
    }

    r.reduce (juce::jmin (maxHorizontalReduce, area.getWidth() / areaWidthReduceDivisor), 0);
}

static void drawMenuItemIcon (ENDLookAndFeel& laf,
                               juce::Graphics& g,
                               juce::Rectangle<int>& r,
                               const juce::PopupMenu::Item& item)
{
    constexpr float iconToTextSpacingRatio { 0.5f };
    constexpr float tickStrokeWidth { 2.0f };
    constexpr float tickInsetDivisor { 3.0f };

    auto maxFontHeight { static_cast<float> (r.getHeight()) };
    auto iconArea { r.removeFromLeft (juce::roundToInt (maxFontHeight)).toFloat() };

    const auto* svg { getMenuItemSVG (item.itemID) };

    if (svg != nullptr)
    {
        auto path { jam::Svg::getPath (svg, iconArea) };
        g.fillPath (path);
        r.removeFromLeft (juce::roundToInt (maxFontHeight * iconToTextSpacingRatio));
    }
    else if (item.isTicked)
    {
        auto tick { laf.getTickShape (1.0f) };
        auto stroke { juce::PathStrokeType (tickStrokeWidth) };
        auto delta { iconArea.getWidth() / tickInsetDivisor };
        g.strokePath (tick, stroke, tick.getTransformToScaleToFit (iconArea.reduced (delta).toFloat(), true));
    }
}

static void drawMenuItemArrow (ENDLookAndFeel& laf,
                                juce::Graphics& g,
                                juce::Rectangle<int>& r,
                                const juce::PopupMenu::Item& item)
{
    constexpr float arrowHeightRatio { 0.6f };
    constexpr float arrowTipHorizontalRatio { 0.6f };
    constexpr float arrowVerticalHalfRatio { 0.5f };
    constexpr float arrowStrokeWidth { 2.0f };

    const bool hasSubMenu { item.subMenu != nullptr and item.subMenu->getNumItems() > 0 };

    if (hasSubMenu)
    {
        auto arrowHeight { arrowHeightRatio * laf.getPopupMenuFont().getAscent() };

        auto x { static_cast<float> (r.removeFromRight (static_cast<int> (arrowHeight)).getX()) };
        auto arrowCentreY { static_cast<float> (r.getCentreY()) };

        juce::Path path;
        path.startNewSubPath (x, arrowCentreY - arrowHeight * arrowVerticalHalfRatio);
        path.lineTo (x + arrowHeight * arrowTipHorizontalRatio, arrowCentreY);
        path.lineTo (x, arrowCentreY + arrowHeight * arrowVerticalHalfRatio);

        g.strokePath (path, juce::PathStrokeType (arrowStrokeWidth));
    }
}

static void drawMenuItemText (juce::Graphics& g,
                              juce::Rectangle<int>& r,
                              const juce::PopupMenu::Item& item,
                              const juce::Font& font)
{
    constexpr int textRightMargin { 3 };
    constexpr float shortcutFontScale { 0.75f };
    constexpr float shortcutHorizontalScale { 0.95f };

    r.removeFromRight (textRightMargin);
    g.drawFittedText (item.text, r, juce::Justification::centredLeft, 1);

    if (item.shortcutKeyDescription.isNotEmpty())
    {
        auto shortcutFont { font.withPointHeight (font.getHeightInPoints() * shortcutFontScale) };
        shortcutFont.setHorizontalScale (shortcutHorizontalScale);
        g.setFont (shortcutFont);

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
        constexpr int separatorHorizontalReduce { 5 };
        constexpr float separatorVerticalCenterRatio { 0.5f };
        constexpr float separatorRoundingOffset { 0.5f };
        constexpr int separatorThickness { 1 };
        constexpr float separatorAlpha { 0.3f };

        auto r { area.reduced (separatorHorizontalReduce, 0) };
        r.removeFromTop (juce::roundToInt ((static_cast<float> (r.getHeight()) * separatorVerticalCenterRatio)
                                           - separatorRoundingOffset));

        g.setColour (findColour (juce::PopupMenu::textColourId).withAlpha (separatorAlpha));
        g.fillRect (r.removeFromTop (separatorThickness));
    }
    else
    {
        constexpr int itemAreaReduce { 1 };

        auto r { area.reduced (itemAreaReduce) };

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
        bounds = applyRotation (g, bounds, getVerticalTextTransform (map::Position::left, bounds.getWidth(), bounds.getHeight())
                                               .translated (bounds.getX(), bounds.getY()));

    jam::Svg::Flex::paint (g, *this, graphics.at (Id::resizerBar), bounds);
}

void ENDLookAndFeel::drawPaneOutline (juce::Graphics& g, juce::Component& pane)
{
    g.setColour (findColour (isPaneFocused (pane) ? jam::PaneComponent::focusedOutlineColourId
                                                  : jam::PaneComponent::outlineColourId));
    g.drawRoundedRectangle (getPaneArea (pane, getPanePadding()), jam::PaneComponent::cornerSize, jam::PaneComponent::lineThickness);
}

void ENDLookAndFeel::drawPaneBackground (juce::Graphics& g, juce::Component& pane)
{
    g.setColour (findColour (isPaneFocused (pane) ? jam::PaneComponent::focusedBackgroundColourId
                                                  : jam::PaneComponent::backgroundColourId));
    g.fillRoundedRectangle (getPaneArea (pane, getPanePadding()), jam::PaneComponent::cornerSize);
}

//==============================================================================
void ENDLookAndFeel::registerTypeface()
{
    auto registerFont = [this] (const void* data, int size)
    {
        auto typeface { juce::Typeface::createSystemTypefaceFor (data, size) };
        auto key { getTypefaceKey (typeface->getName(), typeface->getStyle()) };
        typefaces.addOrReplace (key, typeface);
        jam::Typeface::getInstance()->registerTypeface (typeface, data, static_cast<size_t> (size));
    };

    registerFont (BinaryData::DisplayBold_ttf, BinaryData::DisplayBold_ttfSize);
    registerFont (BinaryData::DisplayBook_ttf, BinaryData::DisplayBook_ttfSize);
    registerFont (BinaryData::DisplayMedium_ttf, BinaryData::DisplayMedium_ttfSize);
    registerFont (BinaryData::DisplayMonoBold_ttf, BinaryData::DisplayMonoBold_ttfSize);
    registerFont (BinaryData::DisplayMonoBook_ttf, BinaryData::DisplayMonoBook_ttfSize);
    registerFont (BinaryData::DisplayMonoMedium_ttf, BinaryData::DisplayMonoMedium_ttfSize);
}

void ENDLookAndFeel::setColours()
{
    static const jam::Array<std::tuple<juce::Identifier, juce::Identifier, int>> colourIds
    {
        { jam::Format::toType (Id::scrollbar), Id::thumb, juce::ScrollBar::thumbColourId },
        { jam::Format::toType (Id::scrollbar), Id::track, juce::ScrollBar::trackColourId },
        { jam::Format::toType (Id::button), Id::button, juce::TextButton::buttonColourId },
        { jam::Format::toType (Id::button), Id::buttonOn, juce::TextButton::buttonOnColourId },
        { jam::Format::toType (Id::button), Id::textOff, juce::TextButton::textColourOffId },
        { jam::Format::toType (Id::button), Id::textOn, juce::TextButton::textColourOnId },
        { jam::Format::toType (Id::overlay), Id::background, juce::Label::backgroundColourId },
        { jam::Format::toType (Id::overlay), Id::text, juce::Label::textColourId },
        { jam::Format::toType (Id::pane), Id::outline, jam::PaneComponent::outlineColourId },
        { jam::Format::toType (Id::pane), Id::focusedOutline, jam::PaneComponent::focusedOutlineColourId },
        { jam::Format::toType (Id::pane), Id::background, jam::PaneComponent::backgroundColourId },
        { jam::Format::toType (Id::pane), Id::focusedBackground, jam::PaneComponent::focusedBackgroundColourId }
    };

    for (const auto& [tableType, rowKey, colourId] : colourIds)
        setColour (colourId, jam::ColourScheme::toColour (config.getRowValue (tableType, rowKey)));
}

void ENDLookAndFeel::applyColours()
{
    setColours();
    setPopupMenuColours();
}

void ENDLookAndFeel::setPopupMenuColours()
{
    const auto windowColour { jam::ColourScheme::toColour (config.getRowValue (jam::Format::toType (Id::window), Id::background)) };
    const float menuOpacity { config.getRowValue (jam::Format::toType (Id::menu), Id::opacity) };
    const auto textColour { jam::ColourScheme::toColour (config.getRowValue (jam::Format::toType (Id::menu), Id::text)) };
    const auto highlightColour { jam::ColourScheme::toColour (config.getRowValue (jam::Format::toType (Id::menu), Id::highlight)) };

    setColour (juce::PopupMenu::backgroundColourId, windowColour.withAlpha (menuOpacity));
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, highlightColour);
    setColour (juce::PopupMenu::highlightedTextColourId, textColour);
}

void ENDLookAndFeel::loadGraphics()
{
    jam::HashMap<juce::Identifier, jam::Svg::Flex::Segments> newGraphics;

    jam::Model::forEachProperty (jam::Model::getChildWithName (config.state, jam::Format::toType (Id::flex)),
        [this, &newGraphics] (const juce::Identifier& propName, const juce::var& value)
        {
            newGraphics.addOrReplace (propName, jam::Svg::Flex::getSegments (value.toString(), colourScheme));
        });

    graphics = std::move (newGraphics);
}

void ENDLookAndFeel::registerEvents()
{
    events.add<juce::ValueTree&> (jam::Format::toType (Id::theme),
                                  [this] (juce::ValueTree&)
                                  {
                                      applyColours();
                                      loadGraphics();
                                  });

    for (const auto& type : { jam::Format::toType (Id::scrollbar), jam::Format::toType (Id::button), jam::Format::toType (Id::overlay), jam::Format::toType (Id::pane) })
        events.add<juce::ValueTree&> (type, [this] (juce::ValueTree&) { setColours(); });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::appearance), [this] (juce::ValueTree&) { applyColours(); });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::menu),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPopupMenuColours();
                                  });
}
