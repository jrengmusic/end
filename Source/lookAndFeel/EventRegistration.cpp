#include "LookAndFeel.h"

namespace end
{
/*____________________________________________________________________________*/

void LookAndFeel::registerTypeface()
{
    // JUCE side: create Ptrs from embedded binaries so font name lookup resolves
    // without requiring system-installed fonts. Lambda avoids 6x repetition.
    auto add = [this] (const void* data, int size)
    {
        auto ptr { juce::Typeface::createSystemTypefaceFor (data, size) };
        typefaces.addOrReplace (ptr->getName(), ptr);
    };

    add (jam::fonts::DisplayBold_ttf, jam::fonts::DisplayBold_ttfSize);
    add (jam::fonts::DisplayBook_ttf, jam::fonts::DisplayBook_ttfSize);
    add (jam::fonts::DisplayMedium_ttf, jam::fonts::DisplayMedium_ttfSize);
    add (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);
    add (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);
    add (jam::fonts::DisplayMonoMedium_ttf, jam::fonts::DisplayMonoMedium_ttfSize);

    // STUB: jam::Typeface removed — glyph pipeline deleted. Code typeface registration
    // will be re-added when the new glyph pipeline is wired.
    // jam::Typeface::registerTypeface (...) removed.
}

void LookAndFeel::initialiseColours()
{
    colourMap = jam::ColourMap::fromValueTree (config.state);

    // STUB: jam::CodeView and jam::CaretComponent removed — glyph pipeline deleted.
    // setColourId (IDtype::code, jam::ID::text, jam::CodeView::textColourId);
    // setColourId (IDtype::code, jam::ID::background, jam::CodeView::backgroundColourId);
    // setColourId (IDtype::code, ID::caret, jam::CaretComponent::caretColourId);
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

    setColours (config.state);
}

void LookAndFeel::loadGraphics()
{
    graphics.clear();

    auto graphicsTree { jam::Model::getChildWithName (config.state, IDtype::flex) };

    if (graphicsTree.isValid())
    {
        jam::Model::forEachProperty (graphicsTree,
            [this] (const juce::Identifier& propName, const juce::var& value)
            {
                if (value.isString())
                {
                    auto stem { propName.toString() };
                    auto suffix { stem.fromLastOccurrenceOf ("_", false, false) };

                    juce::Identifier id { suffix.isNotEmpty()
                        and jam::map::ButtonState::getInstance()->contains (suffix)
                            ? suffix : stem };

                    graphics.addOrReplace (id,
                        jam::SVG::Flex::getSegments (value.toString(), colourMap));
                }
            });
    }
}

void LookAndFeel::registerEvents()
{
    events.add<juce::ValueTree&> (ID::theme,
                                  [this] (juce::ValueTree&)
                                  {
                                      initialiseColours();
                                      loadGraphics();
                                  });

    events.add<juce::ValueTree&> (IDtype::code,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::scrollbar,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::tab,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (jam::IDtype::button,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (jam::IDtype::overlay,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::pane,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::statusBar,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::hint,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
