#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

const jam::HashMap<juce::Identifier, jam::HashMap<juce::Identifier, std::vector<int>>>
    LookAndFeel::colourIds {
        { IDtype::tab,
          {
              { jam::ID::background,    { jam::button::Bar::backgroundColourId             } },
              { ID::highlight,          { jam::button::Bar::highlightColourId              } },
              { jam::ID::outline,       { jam::button::Bar::outlineColourId                } },
              { jam::ID::button,        { juce::TextButton::buttonColourId                 } },
              { ID::buttonOn,           { juce::TextButton::buttonOnColourId               } },
              { ID::textOff,            { juce::TextButton::textColourOffId                } },
              { ID::textOn,             { juce::TextButton::textColourOnId                 } },
          } },
        { jam::IDtype::colours,
          {
              { ID::caret,              { juce::CaretComponent::caretColourId,
                                          jam::CodeView::caretColourId                     } },
              { jam::ID::text,          { jam::CodeView::textColourId                      } },
              { jam::ID::background,    { jam::CodeView::backgroundColourId                } },
              { ID::highlight,          { juce::TextEditor::highlightColourId              } },
              { ID::editorBackground,   { juce::TextEditor::backgroundColourId             } },
              { ID::editorOutline,      { juce::TextEditor::outlineColourId                } },
              { ID::scrollbarThumb,     { juce::ScrollBar::thumbColourId                   } },
              { ID::scrollbarTrack,     { juce::ScrollBar::trackColourId                   } },
              { ID::selectionCursor,    { selectionCursorColourId                          } },
              { ID::statusBar,          { statusBarBackgroundColourId                      } },
              { ID::statusBarLabelBg,   { statusBarLabelBackgroundColourId                 } },
              { ID::statusBarLabelFg,   { statusBarLabelTextColourId                       } },
              { ID::statusBarSpinner,   { statusBarSpinnerColourId                         } },
              { ID::hintLabelBg,        { hintLabelBgColourId                              } },
              { ID::hintLabelFg,        { hintLabelFgColourId                              } },
          } },
        { IDtype::window,
          {
              { jam::ID::background,    { juce::ResizableWindow::backgroundColourId        } },
          } },
        { IDtype::pane,
          {
              { ID::barColour,          { paneBarColourId                                  } },
              { ID::barHighlight,       { paneBarHighlightColourId                         } },
          } },
        { jam::IDtype::overlay,
          {
              { jam::ID::background,    { juce::Label::backgroundColourId                  } },
              { jam::ID::text,          { juce::Label::textColourId                        } },
          } },
    };

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColours();
    loadGraphics();
    config.addListener (this);
}

LookAndFeel::~LookAndFeel() { config.removeListener (this); };

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (colourIds.contains (tree.getType())
        and colourIds.at (tree.getType()).contains (property))
    {
        setColours();
    }
}

//==============================================================================
void LookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& barComp)
{
    juce::ignoreUnused (g, barComp);
    // TODO: Flex paint
}

void LookAndFeel::drawBarIndicator (juce::Graphics& g, juce::Component& indicatorComp)
{
    juce::ignoreUnused (g, indicatorComp);
    // TODO: Flex paint
}

void LookAndFeel::drawTabButton (juce::Graphics& g,
                                 juce::Button& button,
                                 bool isMouseOver,
                                 bool isMouseDown)
{
    juce::ignoreUnused (isMouseOver, isMouseDown);
    auto colourId { button.getToggleState() ? juce::TextButton::buttonOnColourId
                                            : juce::TextButton::buttonColourId };
    g.fillAll (findColour (colourId));
}

juce::Font LookAndFeel::getTabFont() const
{
    auto tab { config.getDisplay (IDtype::tab) };
    auto fontFamily { tab.getProperty (ID::fontFamily) };
    auto fontSize { tab.getProperty (ID::fontSize) };

    return juce::Font { juce::FontOptions().withName (fontFamily).withPointHeight (fontSize) };
}

juce::Font LookAndFeel::getTabFont (int barDepth) const
{
    juce::ignoreUnused (barDepth);
    return getTabFont();
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

//==============================================================================
void LookAndFeel::setColours()
{
    jam::Model::applyFunctionRecursively (
        config.state,
        [this] (const juce::ValueTree& tree)
        {
            if (colourIds.contains (tree.getType()))
            {
                for (auto& [property, ids] : colourIds.at (tree.getType()))
                {
                    if (tree.hasProperty (property))
                    {
                        for (int colourId : ids)
                            setColour (colourId, jam::Model::toColour (tree.getProperty (property)));
                    }
                }
            }

            return false;
        });
}

void LookAndFeel::loadGraphics()
{
    // TODO: rebuild Flex instances from config SVG files
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
