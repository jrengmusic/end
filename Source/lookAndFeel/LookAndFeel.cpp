#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

//==============================================================================
LookAndFeel::LookAndFeel()
{
    initialiseColours();
    loadGraphics();
    config.addListener (this);
}

void LookAndFeel::initialiseColours()
{
    colourMap = jam::ColourMap::fromValueTree (config.state);

    setColourId (IDtype::tab, jam::ID::background, jam::button::Bar::backgroundColourId);
    setColourId (IDtype::tab, ID::highlight, jam::button::Bar::highlightColourId);
    setColourId (IDtype::tab, jam::ID::outline, jam::button::Bar::outlineColourId);
    setColourId (IDtype::tab, jam::ID::button, juce::TextButton::buttonColourId);
    setColourId (IDtype::tab, ID::buttonOn, juce::TextButton::buttonOnColourId);
    setColourId (IDtype::tab, ID::textOff, juce::TextButton::textColourOffId);
    setColourId (IDtype::tab, ID::textOn, juce::TextButton::textColourOnId);
    setColourId (jam::IDtype::colours, ID::caret, jam::CaretComponent::caretColourId);
    setColourId (jam::IDtype::colours, jam::ID::text, jam::CodeView::textColourId);
    setColourId (jam::IDtype::colours, jam::ID::background, jam::CodeView::backgroundColourId);
    setColourId (jam::IDtype::colours, ID::highlight, juce::TextEditor::highlightColourId);
    setColourId (jam::IDtype::colours, ID::editorBackground, juce::TextEditor::backgroundColourId);
    setColourId (jam::IDtype::colours, ID::editorOutline, juce::TextEditor::outlineColourId);
    setColourId (jam::IDtype::colours, ID::scrollbarThumb, juce::ScrollBar::thumbColourId);
    setColourId (jam::IDtype::colours, ID::scrollbarTrack, juce::ScrollBar::trackColourId);
    setColourId (jam::IDtype::colours, ID::selectionCursor, selectionCursorColourId);
    setColourId (jam::IDtype::colours, ID::statusBar, statusBarBackgroundColourId);
    setColourId (jam::IDtype::colours, ID::statusBarLabelBg, statusBarLabelBackgroundColourId);
    setColourId (jam::IDtype::colours, ID::statusBarLabelFg, statusBarLabelTextColourId);
    setColourId (jam::IDtype::colours, ID::statusBarSpinner, statusBarSpinnerColourId);
    setColourId (jam::IDtype::colours, ID::hintLabelBg, hintLabelBgColourId);
    setColourId (jam::IDtype::colours, ID::hintLabelFg, hintLabelFgColourId);
    setColourId (IDtype::window, jam::ID::background, juce::ResizableWindow::backgroundColourId);
    setColourId (IDtype::pane, ID::barColour, paneBarColourId);
    setColourId (IDtype::pane, ID::barHighlight, paneBarHighlightColourId);
    setColourId (jam::IDtype::overlay, jam::ID::background, juce::Label::backgroundColourId);
    setColourId (jam::IDtype::overlay, jam::ID::text, juce::Label::textColourId);

    setColours (config.state);
}

LookAndFeel::~LookAndFeel() { config.removeListener (this); };

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (contains (tree, property))
        setColours (config.state);

    if (tree.getType() == IDtype::graphics or tree.getType() == IDtype::tabButton)
    {
        loadGraphics();

        for (int i { 0 }; i < juce::Desktop::getInstance().getNumComponents(); ++i)
            juce::Desktop::getInstance().getComponent (i)->repaint();
    }
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

    // Sparse bank — paint only when the state slot was authored in graphics.lua.
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
    auto tab { config.getDisplay (IDtype::tab) };
    auto fontFamily { tab.getProperty (ID::fontFamily) };
    auto fontSize { tab.getProperty (ID::fontSize) };
    const float kerning { tab.getProperty (ID::kerningFactor) };

    return juce::Font { juce::FontOptions()
                            .withName (fontFamily)
                            .withPointHeight (fontSize)
                            .withKerningFactor (kerning) };
}

int LookAndFeel::getTabPadding (const juce::Font& font) const
{
    juce::ignoreUnused (font);
    return config.getDisplay (IDtype::tab).getProperty (jam::ID::padding);
}

juce::String LookAndFeel::getTabText (const juce::String& tabName) const
{
    auto tab { config.getDisplay (IDtype::tab) };

    if (bool uppercase { tab.getProperty (ID::uppercase) })
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
    auto configGraphics { config.state.getChildWithName (IDtype::graphics) };
    auto directory { config::Graphics::path.getChildFile (
        configGraphics.getProperty (jam::ID::path).toString()) };
    const auto* tab { colourMap.getChildWithName (IDtype::tab) };
    assert (tab != nullptr and "loadGraphics: tab configGraphics missing from colourMap");

    // Tab bar background
    {
        auto svg { directory.getChildFile (configGraphics.getProperty (ID::tabBar).toString())
                       .loadFileAsString() };
        graphics.insert_or_assign (ID::tabBar, jam::SVG::Flex::getSegments (svg, *tab));
    }

    // Tab highlight
    {
        auto svg { directory.getChildFile (configGraphics.getProperty (ID::tabHighlight).toString())
                       .loadFileAsString() };
        graphics.insert_or_assign (ID::tabHighlight, jam::SVG::Flex::getSegments (svg, *tab));
    }

    // Tab button state bank — sparse 8-slot: all ButtonState slots iterated,
    // unauthored states (empty filename) simply absent from the map.
    {
        auto tabButtonNode { configGraphics.getChildWithName (IDtype::tabButton) };

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
