#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

//==============================================================================
LookAndFeel::LookAndFeel()
{
    registerTypeface();
    initialiseColours();
    loadGraphics();
    config.addListener (this);
    theme.addListener (this);
}

void LookAndFeel::registerTypeface()
{
    // JUCE side: create Ptrs from embedded binaries so font name lookup resolves
    // without requiring system-installed fonts. Lambda avoids 6x repetition.
    auto add = [this] (const void* data, int size)
    {
        auto ptr { juce::Typeface::createSystemTypefaceFor (data, size) };
        typefaces.insert_or_assign (ptr->getName(), ptr);
    };

    add (jam::fonts::DisplayBold_ttf, jam::fonts::DisplayBold_ttfSize);
    add (jam::fonts::DisplayBook_ttf, jam::fonts::DisplayBook_ttfSize);
    add (jam::fonts::DisplayMedium_ttf, jam::fonts::DisplayMedium_ttfSize);
    add (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);
    add (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);
    add (jam::fonts::DisplayMonoMedium_ttf, jam::fonts::DisplayMonoMedium_ttfSize);

    // jam side: build the composite code typeface from theme, with emoji + nerd-font fallbacks
    // and a bold style variant.
    juce::String fontFamily { theme.getValue (IDtype::code, ID::fontFamily) };
    float fontSize { theme.getValue (IDtype::code, ID::fontSize) };

    auto typeface { std::make_unique<jam::Typeface> (fontFamily,
#if JUCE_MAC
                                                     "Apple Color Emoji",
#elif JUCE_WINDOWS
                                                     "Segoe UI Emoji",
#else
                                                     "Noto Color Emoji",
#endif
                                                     fontSize) };

    typeface->addFallbackFont (
        jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);

    typeface->addFallbackFont (
        BinaryData::SymbolsNerdFontRegular_ttf, BinaryData::SymbolsNerdFontRegular_ttfSize);

    typeface->registerStyleFont (
        jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);

    jam::Typeface::registerTypeface (fontFamily, std::move (typeface));
}

void LookAndFeel::initialiseColours()
{
    colourMap = jam::ColourMap::fromValueTree (theme.state);

    setColourId (IDtype::code, jam::ID::text, jam::CodeView::textColourId);
    setColourId (IDtype::code, jam::ID::background, jam::CodeView::backgroundColourId);
    setColourId (IDtype::code, ID::caret, jam::CaretComponent::caretColourId);
    setColourId (IDtype::code, ID::highlight, juce::TextEditor::highlightColourId);
    setColourId (IDtype::code, ID::selectionCursor, selectionCursorColourId);
    setColourId (IDtype::code, ID::editorBackground, juce::TextEditor::backgroundColourId);
    setColourId (IDtype::code, ID::editorOutline, juce::TextEditor::outlineColourId);
    setColourId (IDtype::scrollbar, ID::thumb, juce::ScrollBar::thumbColourId);
    setColourId (IDtype::scrollbar, ID::track, juce::ScrollBar::trackColourId);
    setColourId (IDtype::tab, jam::ID::background, jam::button::Bar::backgroundColourId);
    setColourId (IDtype::tab, ID::highlight, jam::button::Bar::highlightColourId);
    setColourId (IDtype::tab, jam::ID::outline, jam::button::Bar::outlineColourId);
    setColourId (jam::IDtype::button, jam::ID::button, juce::TextButton::buttonColourId);
    setColourId (jam::IDtype::button, ID::buttonOn, juce::TextButton::buttonOnColourId);
    setColourId (jam::IDtype::button, ID::textOff, juce::TextButton::textColourOffId);
    setColourId (jam::IDtype::button, ID::textOn, juce::TextButton::textColourOnId);
    setColourId (jam::IDtype::overlay, jam::ID::background, juce::Label::backgroundColourId);
    setColourId (jam::IDtype::overlay, jam::ID::text, juce::Label::textColourId);
    setColourId (IDtype::pane, ID::barColour, paneBarColourId);
    setColourId (IDtype::pane, ID::barHighlight, paneBarHighlightColourId);
    setColourId (IDtype::statusBar, jam::ID::background, statusBarBackgroundColourId);
    setColourId (IDtype::statusBar, ID::labelBackground, statusBarLabelBackgroundColourId);
    setColourId (IDtype::statusBar, ID::labelText, statusBarLabelTextColourId);
    setColourId (IDtype::statusBar, ID::spinner, statusBarSpinnerColourId);
    setColourId (IDtype::hint, jam::ID::background, hintLabelBgColourId);
    setColourId (IDtype::hint, jam::ID::text, hintLabelFgColourId);

    setColours (theme.state);
}

LookAndFeel::~LookAndFeel()
{
    theme.removeListener (this);
    config.removeListener (this);
}

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == ID::theme)
        initialiseColours();

    if (contains (tree, property))
        setColours (theme.state);

    if (tree.getType() == IDtype::graphics or tree.getType() == IDtype::tabButton)
        loadGraphics();
}

//==============================================================================
void LookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& bar)
{
    jam::SVG::Flex::paint (g, bar, graphics.at (ID::tabBar));
}

void LookAndFeel::drawBarHighlight (juce::Graphics& g, juce::Component& highlight)
{
    jam::SVG::Flex::paint (g, highlight, graphics.at (ID::tabHighlight));
}

void LookAndFeel::drawTabButton (juce::Graphics& g,
                                 juce::Button& button,
                                 bool isMouseOver,
                                 bool isMouseDown)
{
    const auto state { jam::SVG::Button::getState (
        button, isMouseOver, isMouseDown, jam::map::ButtonState::get().size()) };
    const juce::Identifier stateId { jam::map::ButtonState::get (state) };

    // Sparse bank — paint only when the state slot was authored in theme.lua graphics section.
    if (graphics.contains (stateId))
        jam::SVG::Flex::paint (g, button, graphics.at (stateId));

    g.setFont (getTabFont());
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId
                                                            : juce::TextButton::textColourOffId));
    g.drawText (
        getTabText (button.getButtonText()), button.getLocalBounds(), juce::Justification::centred);
}

juce::Font LookAndFeel::getTabFont() const
{
    auto fontFamily { theme.getValue (IDtype::tab, ID::fontFamily) };
    auto fontSize { theme.getValue (IDtype::tab, ID::fontSize) };
    const float kerning { theme.getValue (IDtype::tab, ID::kerningFactor) };

    return juce::Font { juce::FontOptions()
                            .withName (fontFamily)
                            .withPointHeight (fontSize)
                            .withKerningFactor (kerning) };
}

int LookAndFeel::getTabPadding() const { return theme.getValue (IDtype::tab, ID::textPadding); }

juce::BorderSize<int> LookAndFeel::getTabBarPadding() const
{
    // CSS order { top, right, bottom, left }; BorderSize ctor is (top, left, bottom, right).
    auto [top, right, bottom, left] = theme.getInt16 (IDtype::tab, jam::ID::padding);

    return juce::BorderSize<int> { top, left, bottom, right };
}

LookAndFeel::Glass LookAndFeel::getWindowGlass() const
{
    auto colour { jam::Model::toColour (theme.getValue (IDtype::window, jam::ID::background)) };
    int blur { theme.getValue (IDtype::window, ID::blurRadius) };
    int fx { 0 };

#if JUCE_MAC
    auto name { theme.getValue (IDtype::windowFx, ID::mac).toString() };
#elif JUCE_WINDOWS
    auto name { theme.getValue (IDtype::windowFx, ID::win).toString() };
#endif

    if (jam::map::WindowFX::getInstance()->contains (name))
        fx = jam::map::WindowFX::get (name);

    return Glass::pack (colour, blur, fx);
}

juce::String LookAndFeel::getTabText (const juce::String& tabName) const
{
    if (bool uppercase { theme.getValue (IDtype::tab, ID::uppercase) })
        return tabName.toUpperCase();

    return tabName;
}

//==============================================================================
void LookAndFeel::drawStretchableLayoutResizerBar (juce::Graphics& g,
                                                   int w,
                                                   int h,
                                                   bool /*isVertical*/,
                                                   bool isMouseOver,
                                                   bool isMouseDown)
{
    if (isMouseOver or isMouseDown)
        g.setColour (findColour (paneBarHighlightColourId));
    else
        g.setColour (findColour (paneBarColourId));

    g.fillRect (0, 0, w, h);
}

void LookAndFeel::loadGraphics()
{
    auto themeName { config.getValue (IDtype::end, ID::theme).toString() };
    auto themeGraphics { jam::Model::getChildWithName (theme.state, IDtype::graphics) };
    auto directory { config::Theme::getPath (themeName).getChildFile (jam::IDref::graphics) };
    const auto* tab { colourMap.getChildWithName (IDtype::tab) };
    assert (tab != nullptr and "loadGraphics: tab missing from colourMap");

    // Tab bar background
    {
        auto svg { directory.getChildFile (themeGraphics.getProperty (ID::tabBar).toString())
                       .loadFileAsString() };
        graphics.insert_or_assign (ID::tabBar, jam::SVG::Flex::getSegments (svg, *tab));
    }

    // Tab highlight
    {
        auto svg { directory.getChildFile (themeGraphics.getProperty (ID::tabHighlight).toString())
                       .loadFileAsString() };
        graphics.insert_or_assign (ID::tabHighlight, jam::SVG::Flex::getSegments (svg, *tab));
    }

    // Tab button state bank — sparse 8-slot: all ButtonState slots iterated,
    // unauthored states (empty filename) simply absent from the map.
    {
        auto tabButtonNode { themeGraphics.getChildWithName (IDtype::tabButton) };

        for (size_t slot { 0 }; slot < jam::map::ButtonState::get().size(); ++slot)
        {
            const juce::Identifier stateId { jam::map::ButtonState::get (static_cast<int> (slot)) };
            const auto fileName { tabButtonNode.getProperty (stateId).toString() };

            if (fileName.isNotEmpty())
            {
                auto svg { directory.getChildFile (fileName).loadFileAsString() };
                graphics.insert_or_assign (stateId, jam::SVG::Flex::getSegments (svg, *tab));
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
