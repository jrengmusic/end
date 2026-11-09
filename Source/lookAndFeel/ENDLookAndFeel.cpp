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

ENDLookAndFeel::ENDLookAndFeel()
{
    registerEvents();
    config.addListener (this);
    juce::Desktop::getInstance().addDarkModeSettingListener (this);
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
    return jam::StyleManager::getInstance()->getFont (Id::tab);
}

juce::FontOptions ENDLookAndFeel::getOverlayFont() const
{
    return jam::StyleManager::getInstance()->getFont (Id::overlay);
}

juce::Font ENDLookAndFeel::getPopupMenuFont() { return jam::StyleManager::getInstance()->getFont (Id::tab); }

int ENDLookAndFeel::getTabBarDepth (const jam::TabbedComponent& tabs) const noexcept
{
    const float depth { jam::StyleManager::getInstance()->getMetrics<float> (jam::StyleManager::getStyleSheet(), Id::tabDepth) };
    const bool alwaysVisible { static_cast<bool> (config.getRowValue (jam::Format::toType (Id::tab), Id::alwaysVisible)) };
    const bool shouldHide { tabs.getNumChildren() <= 1 and not alwaysVisible };
    const int tabBarDepth { juce::roundToInt (juce::Font { getTabFont() }.getHeight() * depth) };

    return shouldHide ? 0 : tabBarDepth;
}

int ENDLookAndFeel::getTabPadding() const { return jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::tabTextPadding); }

int ENDLookAndFeel::getTabPosition() const noexcept
{
    const juce::String position { config.getRowValue (jam::Format::toType (Id::tab), Id::position) };
    return map::Position::getInstance()->get (position);
}

juce::String ENDLookAndFeel::getTabText (const juce::String& tabName) const
{
    if (static_cast<bool> (config.getRowValue (jam::Format::toType (Id::tab), Id::uppercase)))
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
int ENDLookAndFeel::getPaneEdgeSize() const noexcept
{
    return jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::paneComponentResizeBarThickness);
}

int ENDLookAndFeel::getPanePadding() const noexcept
{
    return jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::paneComponentPadding);
}

//==============================================================================
juce::Typeface::Ptr ENDLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    static constexpr const char* bookStyle { "Book" };

    const auto name { font.getTypefaceName() };
    const auto style { font.getTypefaceStyle() };
    const auto fonts { jam::StyleManager::getInstance()->getConfig (jam::Format::toType (Id::fonts)) };

    juce::Typeface::Ptr bookTypeface {};

    for (const auto& face : fonts)
    {
        const auto typeface { jam::StyleManager::getInstance()->getFont (face.getType().toString()).getTypeface() };
        const bool isSameName { typeface->getName().compare (name) == 0 };

        if (isSameName and typeface->getStyle().compare (style) == 0)
            return typeface;

        if (isSameName and typeface->getStyle().compare (bookStyle) == 0)
            bookTypeface = typeface;
    }

    if (bookTypeface != nullptr)
        return bookTypeface;

    return juce::LookAndFeel::getTypefaceForFont (font);
}

juce::BorderSize<int> ENDLookAndFeel::getTabBarPadding() const
{
    return juce::BorderSize<int> { jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::tabPaddingTop),
                                   jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::tabPaddingLeft),
                                   jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::tabPaddingBottom),
                                   jam::StyleManager::getInstance()->getMetrics<int> (jam::StyleManager::getStyleSheet(), Id::tabPaddingRight) };
}

int16_t ENDLookAndFeel::getWindowFX() const
{
    int16_t fx { 0 };

#if JUCE_MAC
    auto name { jam::StyleManager::getInstance()->getWindowStyleString (Id::mac) };
#elif JUCE_WINDOWS
    auto name { jam::StyleManager::getInstance()->getWindowStyleString (Id::win) };
#else
    juce::String name;
#endif

    if (map::WindowFX::getInstance()->contains (name))
        fx = static_cast<int16_t> (map::WindowFX::getInstance()->get (name));

    return fx;
}

float ENDLookAndFeel::getWindowBlur() const noexcept
{
    return jam::StyleManager::getInstance()->getWindowStyle<float> (Id::blur);
}

void ENDLookAndFeel::prepareWindow (juce::Component& window)
{
    const auto colour { findColour (juce::ResizableWindow::backgroundColourId) };
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
                const auto baseColour { safeComponent->findColour (juce::PopupMenu::backgroundColourId) };
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
void ENDLookAndFeel::initialiseStyle()
{
    styleManager.create (jam::ResourceFetchers { BinaryData::fetcher },
                         config.getChildWithName (jam::Format::toType (Id::themes)));

    applyColours();
    applyFontStyles();
    loadGraphics();

    juce::LookAndFeel::setDefaultLookAndFeel (this);
}

void ENDLookAndFeel::applyColours()
{
    const juce::String appearanceText { config.getRowValue (jam::Format::toType (Id::display), Id::appearance) };

    jam::StyleManager::getInstance()->setAppearance (jam::StyleManager::getStyleSheet(), *this, appearanceText);
}

void ENDLookAndFeel::applyFontStyles()
{
    auto* backends { map::FontRasterizerBackend::getInstance() };
    const auto backendName { jam::StyleManager::getInstance()->getFontsStyleString (Id::rasterizer) };
    jassert (backends->contains (backendName));

    jam::CpuGraphicsEngine::getInstance()->setRasterization (static_cast<map::FontRasterizerBackend::value> (backends->get (backendName)),
                                                             jam::StyleManager::getInstance()->getFontsStyle<float> (Id::gamma),
                                                             jam::StyleManager::getInstance()->getFontsStyle<float> (Id::contrast));

    jam::CpuGraphicsEngine::getInstance()->setEmbolden (jam::StyleManager::getInstance()->getFontsStyleString (Id::embolden).equalsIgnoreCase (jam::Format::fromBoolean (true)));
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
                                      jam::StyleManager::getInstance()->registerStyle (config.getChildWithName (jam::Format::toType (Id::themes)));
                                      applyColours();
                                      applyFontStyles();
                                      loadGraphics();
                                  });

    events.add<juce::ValueTree&> (jam::Format::toType (Id::appearance),
                                  [this] (juce::ValueTree&)
                                  {
                                      applyColours();
                                  });
}
