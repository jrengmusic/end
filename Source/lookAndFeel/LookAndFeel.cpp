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

/*____________________________________________________________________________*/
const juce::Colour LookAndFeel::fromRGBA (const juce::String& hexRGBA) noexcept
{
    // Config stores #RRGGBBAA. fromString interprets as AARRGGBB (wrong order).
    // fromRGBA rotates the channels back to the correct RGBA → ARGB layout.
    const auto rgba { juce::Colour::fromString (hexRGBA) };
    const auto red { rgba.getRed() };
    const auto green { rgba.getGreen() };
    const auto blue { rgba.getBlue() };
    const auto alpha { rgba.getAlpha() };
    return juce::Colour::fromRGBA (alpha, red, green, blue);
}

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

    if (isMouseDown)
        colour = colour.brighter (0.2f);
    else if (isMouseOver)
        colour = colour.brighter (0.1f);

    if (button.getToggleState())
    {
        g.setColour (colour);
        g.fillRoundedRectangle (bounds, 4.0f);
    }

    g.setColour (button.findColour (juce::Label::textColourId));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

void LookAndFeel::setColours()
{
    jam::ValueTree::applyFunctionRecursively (config,
        [this] (const juce::ValueTree& tree)
        {
            for (auto& [id, colourId] : colourIds)
            {
                if (tree.hasProperty (id))
                    setColour (colourId, fromRGBA (tree.getProperty (id).toString()));
            }

            return false;
        });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
