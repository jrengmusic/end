#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

const std::unordered_map<juce::Identifier, int> LookAndFeel::colourIds {
    { ID::editorBackground, juce::TextEditor::backgroundColourId      },
    { ID::editorOutline,    juce::TextEditor::outlineColourId         },
    { jam::ID::cursor,      juce::CaretComponent::caretColourId       },
    { ID::selection,        juce::TextEditor::highlightColourId       },
    { ID::scrollbarThumb,   juce::ScrollBar::thumbColourId            },
    { ID::scrollbarTrack,   juce::ScrollBar::trackColourId            },
    { ID::selectionCursor,  selectionCursorColourId                   },
    { ID::statusBar,        statusBarBackgroundColourId               },
    { ID::statusBarLabelBg, statusBarLabelBackgroundColourId          },
    { ID::statusBarLabelFg, statusBarLabelTextColourId                },
    { ID::statusBarSpinner, statusBarSpinnerColourId                  },
    { ID::hintLabelBg,      hintLabelBgColourId                       },
    { ID::hintLabelFg,      hintLabelFgColourId                       },
    { jam::ID::line,        tabLineColourId                           },
    { ID::active,           tabActiveColourId                         },
    { ID::indicator,        tabIndicatorColourId                      },
    { ID::barColour,        paneBarColourId                           },
    { ID::barHighlight,     paneBarHighlightColourId                  },
    { jam::ID::colour,      juce::ResizableWindow::backgroundColourId },
};

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColours();
    config.addListener (this);
}

LookAndFeel::~LookAndFeel() { config.removeListener (this); };

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    setColours();
}

void LookAndFeel::drawTabButton (juce::Graphics& g, juce::Button& button, bool isMouseOver, bool isMouseDown)
{
    auto bounds { button.getLocalBounds().toFloat().reduced (2.0f) };
    auto colour { findColour (tabActiveColourId) };

    if (button.getToggleState())
    {
        if (isMouseDown)
            colour = colour.brighter (0.2f);
        else if (isMouseOver)
            colour = colour.brighter (0.1f);

        g.setColour (colour);
        g.fillRoundedRectangle (bounds, 4.0f);
    }
    else if (isMouseDown)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }
    else if (isMouseOver)
    {
        g.setColour (colour.withAlpha (0.15f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }

    g.setColour (button.findColour (juce::Label::textColourId));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

void LookAndFeel::drawButtonGroupTrack (juce::Graphics& g, juce::Component& group)
{
    auto bounds { group.getLocalBounds().toFloat() };
    g.setColour (findColour (tabLineColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
}

void LookAndFeel::drawButtonGroupSlidingIndicator (juce::Graphics& g, juce::Component& indicator)
{
    auto bounds { indicator.getLocalBounds().toFloat().reduced (2.0f) };
    g.setColour (findColour (tabIndicatorColourId));
    g.fillRoundedRectangle (bounds, 3.0f);
}

void LookAndFeel::setColours()
{
    jam::ValueTree::applyFunctionRecursively (config,
        [this] (const juce::ValueTree& tree)
        {
            for (auto& [id, colourId] : colourIds)
            {
                if (tree.hasProperty (id))
                {
                    setColour (colourId, jam::ValueTree::toColour (tree.getProperty (id)));
                }
            }

            return false;
        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
