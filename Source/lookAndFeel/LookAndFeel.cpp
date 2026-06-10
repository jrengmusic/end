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
    { ID::background,         juce::TabbedComponent::backgroundColourId },
    { ID::inactiveBackground, jam::button::Tab::backgroundColourId      },
    { ID::foreground,         juce::TabbedButtonBar::frontTextColourId  },
    { ID::inactiveText,       juce::TabbedButtonBar::tabTextColourId    },
    { ID::outline,            juce::TabbedButtonBar::tabOutlineColourId },
    { ID::indicator,          jam::button::Bar::indicatorColourId       },
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

void LookAndFeel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    if (property == ID::tabBar or property == ID::tabInactive or property == ID::tabActive)
    {
        loadGraphics();

        auto& desktop { juce::Desktop::getInstance() };

        for (int i { 0 }; i < desktop.getNumComponents(); ++i)
        {
            if (auto* comp { desktop.getComponent (i) })
                comp->repaint();
        }
    }
    else if (colourIds.find (property) != colourIds.end())
    {
        setColours();
    }
}

//==============================================================================
// Bar LAF virtuals.

void LookAndFeel::drawBarBackground (juce::Graphics& g, juce::Component& barComp)
{
    jam::SVG::Flex::paint (
        g, flexGraphics.at (ID::tabBar), barComp.getLocalBounds().toFloat(), *this);
}

void LookAndFeel::drawBarIndicator (juce::Graphics& g, juce::Component& indicatorComp)
{
    jam::SVG::Flex::paint (
        g, flexGraphics.at (ID::tabActive), indicatorComp.getLocalBounds().toFloat(), *this);
}

void LookAndFeel::drawTabButton (juce::Graphics& g,
                                 juce::Button& btn,
                                 bool /*isMouseOver*/,
                                 bool /*isMouseDown*/)
{
    jam::SVG::Flex::paint (
        g, flexGraphics.at (ID::tabInactive), btn.getLocalBounds().toFloat(), *this);

    auto textColour { btn.getToggleState() ? findColour (juce::TabbedButtonBar::frontTextColourId)
                                           : findColour (juce::TabbedButtonBar::tabTextColourId) };

    g.setFont (getTabFont());
    g.setColour (textColour);
    g.drawText (btn.getButtonText(), btn.getLocalBounds(), juce::Justification::centred, true);
}

juce::Font LookAndFeel::getTabFont() const
{
    auto display { config.getChildWithName (IDtype::display) };
    auto tabNode { display.getChildWithName (IDtype::tab) };
    auto family { tabNode.getProperty (ID::family).toString() };
    auto points { static_cast<float> (tabNode.getProperty (ID::size)) };
    return juce::Font { juce::FontOptions().withName (family).withPointHeight (points) };
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

    auto readSvg = [&graphicsNode, &svgPath] (const juce::Identifier& id) -> juce::String
    {
        auto fileName { graphicsNode.getProperty (id).toString() };

        juce::String result;

        if (fileName.isNotEmpty())
            result = svgPath.getChildFile (fileName).loadFileAsString();

        return result;
    };

    flexGraphics.insert_or_assign (
        ID::tabBar, jam::SVG::Flex::getSegments (readSvg (ID::tabBar), colourIds));
    flexGraphics.insert_or_assign (
        ID::tabActive, jam::SVG::Flex::getSegments (readSvg (ID::tabActive), colourIds));
    flexGraphics.insert_or_assign (
        ID::tabInactive, jam::SVG::Flex::getSegments (readSvg (ID::tabInactive), colourIds));
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
