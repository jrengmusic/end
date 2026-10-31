#include "ENDLookAndFeel.h"

void ENDLookAndFeel::registerTypeface()
{
    // One pass, one parse per font (SSOT font list) — creates the Ptr, stores
    // it in typefaces under its {name, style} composite key (typefaceKey()) for
    // name+style lookup (getTypefaceForFont()), and registers the SAME Ptr
    // identity with the atlas, all in a single lambda call per font instead of
    // two separate passes re-parsing the same bytes.
    auto getTypeface = [] (const void* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, size);
    };

    auto registerFont = [this, &getTypeface] (const void* data, int size)
    {
        auto ptr { getTypeface (data, size) };
        auto key { typefaceKey (ptr->getName(), ptr->getStyle()) };
        typefaces.addOrReplace (key, ptr);
        jam::Typeface::getInstance()->registerTypeface (ptr, data, static_cast<size_t> (size));
    };

    registerFont (jam::fonts::DisplayBold_ttf, jam::fonts::DisplayBold_ttfSize);
    registerFont (jam::fonts::DisplayBook_ttf, jam::fonts::DisplayBook_ttfSize);
    registerFont (jam::fonts::DisplayMedium_ttf, jam::fonts::DisplayMedium_ttfSize);
    registerFont (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);
    registerFont (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);
    registerFont (jam::fonts::DisplayMonoMedium_ttf, jam::fonts::DisplayMonoMedium_ttfSize);
}

void ENDLookAndFeel::setColours()
{
    static const std::vector<std::tuple<juce::Identifier, juce::Identifier, int>> colourIds
    {
        { Id::toType (Id::code), Id::caret, juce::CaretComponent::caretColourId },
        { Id::toType (Id::code), Id::selectionCursor, selectionCursorColourId },
        { Id::toType (Id::code), Id::editorBackground, juce::TextEditor::backgroundColourId },
        { Id::toType (Id::code), Id::editorOutline, juce::TextEditor::outlineColourId },
        { Id::toType (Id::scrollbar), Id::thumb, juce::ScrollBar::thumbColourId },
        { Id::toType (Id::scrollbar), Id::track, juce::ScrollBar::trackColourId },
        { Id::toType (Id::tab), Id::background, jam::ButtonBar::backgroundColourId },
        { Id::toType (Id::tab), Id::highlight, jam::ButtonBar::highlightColourId },
        { Id::toType (Id::tab), Id::outline, jam::ButtonBar::outlineColourId },
        { Id::toType (Id::button), Id::button, juce::TextButton::buttonColourId },
        { Id::toType (Id::button), Id::buttonOn, juce::TextButton::buttonOnColourId },
        { Id::toType (Id::button), Id::textOff, juce::TextButton::textColourOffId },
        { Id::toType (Id::button), Id::textOn, juce::TextButton::textColourOnId },
        { Id::toType (Id::overlay), Id::background, juce::Label::backgroundColourId },
        { Id::toType (Id::overlay), Id::text, juce::Label::textColourId },
        { Id::toType (Id::pane), Id::resizeBar, paneBarColourId },
        { Id::toType (Id::pane), Id::resizeBarHighlight, paneBarHighlightColourId },
        { Id::toType (Id::pane), Id::outline, jam::PaneComponent::outlineColourId },
        { Id::toType (Id::pane), Id::focusedOutline, jam::PaneComponent::focusedOutlineColourId }
    };

    for (const auto& [tableType, rowKey, colourId] : colourIds)
        setColour (colourId, jam::ColourScheme::toColour (config.getRowValue (tableType, rowKey)));
}

void ENDLookAndFeel::initialiseColours()
{
    setColours();
    setPopupMenuColours();
}

void ENDLookAndFeel::setPopupMenuColours()
{
    const auto windowColour { jam::ColourScheme::toColour (config.getRowValue (Id::toType (Id::window), Id::background)) };
    const float menuOpacity { config.getRowValue (Id::toType (Id::menu), Id::opacity) };
    const auto textColour { jam::ColourScheme::toColour (config.getRowValue (Id::toType (Id::menu), Id::text)) };
    const auto highlightColour { jam::ColourScheme::toColour (config.getRowValue (Id::toType (Id::menu), Id::highlight)) };

    setColour (juce::PopupMenu::backgroundColourId, windowColour.withAlpha (menuOpacity));
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, highlightColour);
    setColour (juce::PopupMenu::highlightedTextColourId, textColour);
}

void ENDLookAndFeel::loadGraphics()
{
    graphics.clear();

    auto graphicsTree { jam::Model::getChildWithName (config.state, Id::toType (Id::flex)) };

    if (graphicsTree.isValid())
    {
        jam::Model::forEachProperty (graphicsTree,
            [this] (const juce::Identifier& propName, const juce::var& value)
            {
                if (value.isString())
                    graphics.addOrReplace (propName,
                        jam::Svg::Flex::getSegments (value.toString(), colourScheme));
            });
    }
}

void ENDLookAndFeel::registerEvents()
{
    events.add<juce::ValueTree&> (Id::theme,
                                  [this] (juce::ValueTree&)
                                  {
                                      initialiseColours();
                                      loadGraphics();
                                  });

    const auto setColours = [this] (juce::ValueTree&)
    {
        this->setColours();
    };

    events.add<juce::ValueTree&> (Id::toType (Id::code), setColours);
    events.add<juce::ValueTree&> (Id::toType (Id::scrollbar), setColours);
    events.add<juce::ValueTree&> (Id::toType (Id::tab), setColours);
    events.add<juce::ValueTree&> (Id::toType (Id::button), setColours);
    events.add<juce::ValueTree&> (Id::toType (Id::overlay), setColours);
    events.add<juce::ValueTree&> (Id::toType (Id::pane), setColours);

    events.add<juce::ValueTree&> (Id::toType (Id::menu),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPopupMenuColours();
                                  });
}
