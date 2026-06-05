#pragma once
#include <JuceHeader.h>
#include "Identifier.h"
#include "../config/Config.h"

namespace end
{
/*____________________________________________________________________________*/

/** @brief END's LookAndFeel — ValueTree::Listener on config tree.

    valueTreePropertyChanged IS the init and reload path. Main registers
    it on config::Model. Every property change (during build, load, and
    hot-reload) triggers one setColour for the matching colourId.
*/
class LookAndFeel
    : public juce::LookAndFeel_V4
    , public juce::ValueTree::Listener
{
public:
    /** @brief Custom colour IDs for END-specific UI elements not covered by
        the JUCE LookAndFeel_V4 enum. */
    enum ColourIds
    {
        cursorColourId = 0x2000001,
        tabBarBackgroundColourId = 0x2000002,
        tabLineColourId = 0x2000003,
        tabActiveColourId = 0x2000004,
        tabIndicatorColourId = 0x2000005,
        paneBarColourId = 0x2000006,
        paneBarHighlightColourId = 0x2000007,
        statusBarBackgroundColourId = 0x2000100,
        statusBarLabelBackgroundColourId = 0x2000101,
        statusBarLabelTextColourId = 0x2000102,
        statusBarSpinnerColourId = 0x2000103,
        hintLabelBgColourId = 0x2000200,
        hintLabelFgColourId = 0x2000201,
        selectionColourId = 0x2000300,
        selectionCursorColourId = 0x2000301,
    };

    /** @brief Property → colourId map. One entry per lua key. The WINDOW
        tint (jam::ID::colour) is in this same map — its opacity modifier
        is applied inline in valueTreePropertyChanged. */
    inline static const std::unordered_map<juce::Identifier, int> colourIds {
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
    LookAndFeel();
    ~LookAndFeel();

    //==============================================================================
    /** @brief ValueTree::L looks up property in map, sets colour. */
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    //==============================================================================
    static const juce::Colour fromRGBA (const juce::String& hexRGBA) noexcept;

private:
    /** @brief Applies the full colour set from config. Called by the ctor and
        by the ValueTree::Listener path on every property change.  Not part
        of the public API — same shape as end::Window's setStyle(). */
    void setColours();

    //==============================================================================
    juce::ValueTree config { config::Model::get() };
};

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
