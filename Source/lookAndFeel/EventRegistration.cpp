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
        typefaces.insert_or_assign (ptr->getName(), ptr);
    };

    add (jam::fonts::DisplayBold_ttf, jam::fonts::DisplayBold_ttfSize);
    add (jam::fonts::DisplayBook_ttf, jam::fonts::DisplayBook_ttfSize);
    add (jam::fonts::DisplayMedium_ttf, jam::fonts::DisplayMedium_ttfSize);
    add (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);
    add (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);
    add (jam::fonts::DisplayMonoMedium_ttf, jam::fonts::DisplayMonoMedium_ttfSize);

    // jam side: build the composite code typeface from theme, with emoji + nerd-font fallbacks
    // and a bold style variant.
    juce::String fontFamily { theme.getValue (IDtype::code, ID::fontFamily) };
    float fontSize { theme.getValue (IDtype::code, ID::fontSize) };

    auto typeface { std::make_unique<jam::Typeface> (fontFamily,
#if JUCE_MAC
                                                     "Apple Color Emoji",
#elif JUCE_WINDOWS
                                                     "Segoe UI Emoji",
#else
                                                     "Noto Color Emoji",
#endif
                                                     fontSize) };

    typeface->addFallbackFont (
        jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);

    typeface->addFallbackFont (
        BinaryData::SymbolsNerdFontRegular_ttf, BinaryData::SymbolsNerdFontRegular_ttfSize);

    typeface->registerStyleFont (
        jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);

    jam::Typeface::registerTypeface (fontFamily, std::move (typeface));
}

void LookAndFeel::initialiseColours()
{
    colourMap = jam::ColourMap::fromValueTree (theme.state);

    setColourId (IDtype::code, jam::ID::text, jam::CodeView::textColourId);
    setColourId (IDtype::code, jam::ID::background, jam::CodeView::backgroundColourId);
    setColourId (IDtype::code, ID::caret, jam::CaretComponent::caretColourId);
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

    setColours (theme.state);
}

void LookAndFeel::loadGraphics()
{
    auto themeName { config.getValue (IDtype::init, ID::theme).toString() };
    auto directory { config::Themes::getPath (themeName)
                         .getChildFile (jam::IDref::graphics) };

    graphics.clear();

    jam::Model::applyFunctionRecursively (theme.state,
        [&] (const juce::ValueTree& tree)
        {
            auto fileNames { jam::Model::toStringArray (tree.getProperty (ID::graphics)) };
            const auto* colours { colourMap.getChildWithName (tree.getType()) };

            for (const auto& fileName : fileNames)
            {
                if (fileName.isNotEmpty())
                {
                    auto stem { jam::Format::getFilenameWithoutExtension (fileName) };
                    auto suffix { stem.fromLastOccurrenceOf ("_", false, false) };

                    juce::Identifier id { suffix.isNotEmpty()
                        and jam::map::ButtonState::getInstance()->contains (suffix)
                            ? suffix : stem };

                    auto svg { directory.getChildFile (fileName).loadFileAsString() };

                    graphics.insert_or_assign (id,
                        jam::SVG::Flex::getSegments (svg,
                            colours != nullptr ? *colours : colourMap));
                }
            }

            return false;
        });
}

void LookAndFeel::registerEvents()
{
    events.add<juce::ValueTree&> (ID::theme,
                                  [this] (juce::ValueTree&)
                                  {
                                      initialiseColours();
                                  });

    events.add<juce::ValueTree&> (IDtype::graphics,
                                  [this] (juce::ValueTree&)
                                  {
                                      loadGraphics();
                                  });

    events.add<juce::ValueTree&> (IDtype::tabButton,
                                  [this] (juce::ValueTree&)
                                  {
                                      loadGraphics();
                                  });

    events.add<juce::ValueTree&> (IDtype::code,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::scrollbar,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::tab,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (jam::IDtype::button,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (jam::IDtype::overlay,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::pane,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::statusBar,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });

    events.add<juce::ValueTree&> (IDtype::hint,
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (theme.state);
                                  });
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace end
