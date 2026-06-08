#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

const std::unordered_map<juce::Identifier, int> LookAndFeel::colourIds {
    { jam::ID::cursor,        juce::CaretComponent::caretColourId       },
    { ID::editorBackground,   juce::TextEditor::backgroundColourId      },
    { ID::editorOutline,      juce::TextEditor::outlineColourId         },
    { ID::selection,          juce::TextEditor::highlightColourId       },
    { ID::scrollbarThumb,     juce::ScrollBar::thumbColourId            },
    { ID::scrollbarTrack,     juce::ScrollBar::trackColourId            },
    { ID::selectionCursor,    selectionCursorColourId                   },
    { ID::statusBar,          statusBarBackgroundColourId               },
    { ID::statusBarLabelBg,   statusBarLabelBackgroundColourId          },
    { ID::statusBarLabelFg,   statusBarLabelTextColourId                },
    { ID::statusBarSpinner,   statusBarSpinnerColourId                  },
    { ID::hintLabelBg,        hintLabelBgColourId                       },
    { ID::hintLabelFg,        hintLabelFgColourId                       },
    { ID::background,         barBackgroundColourId                     },
    { ID::frontBackground,    frontBackgroundColourId                   },
    { ID::inactiveBackground, inactiveBackgroundColourId                },
    { ID::foreground,         frontTextColourId                         },
    { ID::inactiveText,       inactiveTextColourId                      },
    { ID::outline,            tabOutlineColourId                        },
    { ID::indicator,          indicatorColourId                         },
    { ID::barColour,          paneBarColourId                           },
    { ID::barHighlight,       paneBarHighlightColourId                  },
    { jam::ID::colour,        juce::ResizableWindow::backgroundColourId },
};

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColours();
    config.addListener (this);
}

LookAndFeel::~LookAndFeel() { config.removeListener (this); };

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    setColours();

    if (property == ID::tabBar)
    {
        // SVG reload will go here when the parser is implemented.
        // For now, just repaint to pick up any colour changes.
    }
}

//==============================================================================
// Bar LAF virtuals.

void LookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& bar)
{
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
        config,
        [this] (const juce::ValueTree& tree)
        {
            for (auto& [id, colourId] : colourIds)
            {
                if (tree.hasProperty (id))
                {
                    setColour (colourId, jam::Model::toColour (tree.getProperty (id)));
                }
            }

            return false;
        });
}

//==============================================================================
void LookAndFeel::parseTabBarSvg (const juce::XmlElement& svg, std::vector<Segment>& segments)
{
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
