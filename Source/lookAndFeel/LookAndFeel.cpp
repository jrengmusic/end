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
    setColourId (IDtype::pane, ID::resizeBar, paneBarColourId);
    setColourId (IDtype::pane, ID::resizeBarHighlight, paneBarHighlightColourId);
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
        g.drawText (getTabText (label.getText()),
                    label.getLocalBounds(),
                    juce::Justification::centred);
    }
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

void LookAndFeel::loadGraphics()
{
    auto themeName { config.getValue (IDtype::end, ID::theme).toString() };
    auto directory { config::File::Theme::getPath (themeName)
                         .getChildFile (jam::IDref::graphics) };

    jam::debug::Log::write ("loadGraphics -- theme: " + themeName);
    jam::debug::Log::write ("loadGraphics -- directory: " + directory.getFullPathName());
    jam::debug::Log::write ("loadGraphics -- directory exists: " + juce::String (static_cast<int> (directory.exists())));
    jam::debug::Log::write ("loadGraphics -- theme.state children: " + juce::String (theme.state.getNumChildren()));

    graphics.clear();

    jam::Model::applyFunctionRecursively (theme.state,
        [&] (const juce::ValueTree& tree)
        {
            auto fileNames { jam::Model::toStringArray (tree.getProperty (ID::graphics)) };
            const auto* colours { colourMap.getChildWithName (tree.getType()) };

            jam::debug::Log::write ("  tree: " + tree.getType().toString()
                + " | files: " + juce::String (fileNames.size())
                + " | colours: " + juce::String (static_cast<int> (colours != nullptr)));

            for (const auto& fileName : fileNames)
            {
                if (fileName.isNotEmpty())
                {
                    auto stem { jam::Format::getFilenameWithoutExtension (fileName) };
                    auto suffix { stem.fromLastOccurrenceOf ("_", false, false) };

                    juce::Identifier id { suffix.isNotEmpty()
                        and jam::map::ButtonState::getInstance()->contains (suffix)
                            ? suffix : stem };

                    auto svg { directory.getChildFile (fileName).loadFileAsString() };

                    jam::debug::Log::write ("    " + fileName + " -> id=" + id.toString()
                        + " svgLen=" + juce::String (svg.length()));

                    graphics.insert_or_assign (id,
                        jam::SVG::Flex::getSegments (svg,
                            colours != nullptr ? *colours : colourMap));
                }
            }

            return false;
        });

    jam::debug::Log::write ("loadGraphics -- total entries: " + juce::String (static_cast<int> (graphics.size())));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
