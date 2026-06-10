#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

const jam::HashMap<juce::Identifier, int> LookAndFeel::colourIds {
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
    loadGraphics();
    config.addListener (this);
}

LookAndFeel::~LookAndFeel() { config.removeListener (this); };

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    setColours();

    if (property == ID::tabBar or property == ID::tabInactive or property == ID::tabActive)
    {
        if (auto* arr { tree.getProperty (property).getArray() })
        {
            if (arr->size() >= 2
                and static_cast<int> (arr->getReference (1))
                        == static_cast<int> (jam::File::Watcher::Event::fileUpdated))
            {
                loadGraphics();

                // Reset event to undefined — prevents re-entry past this guard.
                // The reset fires valueTreePropertyChanged again, but the event
                // is undefined on re-entry so the inner if is not entered (bounded).
                juce::Array<juce::var> reset;
                reset.add (arr->getReference (0));
                reset.add (static_cast<int> (jam::File::Watcher::Event::undefined));
                tree.setProperty (property, reset, nullptr);
            }
        }
    }
}

//==============================================================================
// Bar LAF virtuals.

void LookAndFeel::drawBarBackground (juce::Graphics&, juce::Component&) {}

void LookAndFeel::drawBarIndicator (juce::Graphics&, juce::Component&) {}

void LookAndFeel::drawTabButton (juce::Graphics&,
                                 juce::Button&,
                                 bool /*isMouseOver*/,
                                 bool /*isMouseDown*/)
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
        config.state,
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

void LookAndFeel::loadGraphics()
{
    auto graphicsNode { config.state.getChildWithName (IDtype::graphics) };
    auto svgPath { config::Graphics::path.getChildFile (
        graphicsNode.getProperty (jam::ID::path).toString()) };

    auto readSvg = [&] (const juce::Identifier& id) -> juce::String
    {
        auto prop { graphicsNode.getProperty (id) };
        juce::String fileName;

        if (auto* arr { prop.getArray() })
            fileName = arr->getReference (0).toString();
        else
            fileName = prop.toString();

        juce::String result;

        if (fileName.isNotEmpty())
            result = svgPath.getChildFile (fileName).loadFileAsString();

        return result;
    };

    barSvg = readSvg (ID::tabBar);
    indicatorSvg = readSvg (ID::tabActive);
    buttonSvg = readSvg (ID::tabInactive);

    cout (barSvg);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
