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

void LookAndFeel::setColours()
{
    for (auto& [id, colourId] : colourIds)
    {
        for (auto&& c : config)
        {
            if (c.hasProperty (id))
            {
                auto colour { fromRGBA (c.getProperty (id).toString()) };
                setColour (colourId, colour);
            }
        }
    }
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
