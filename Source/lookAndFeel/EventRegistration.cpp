#include "ENDLookAndFeel.h"

static void cascadeLookAndFeelChange()
{
    auto& desktop { juce::Desktop::getInstance() };

    for (int i = 0; i < desktop.getNumComponents(); ++i)
        desktop.getComponent (i)->sendLookAndFeelChange();
}

void ENDLookAndFeel::registerTypeface (jam::GlyphAtlas& atlas)
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

    auto registerFont = [this, &atlas, &getTypeface] (const void* data, int size)
    {
        auto ptr { getTypeface (data, size) };
        auto key { typefaceKey (ptr->getName(), ptr->getStyle()) };
        typefaces.addOrReplace (key, ptr);
        atlas.registerTypeface (ptr, data, static_cast<size_t> (size));

        // jam::Typeface (hb_font_t interning for jam::GlyphArrangement's
        // cmap lookup + tryLigature() shaping) is
        // registered from the SAME (ptr, data, size) triple, one pass.
        jam::Typeface::getInstance()->registerTypeface (ptr, data, static_cast<size_t> (size));
    };

    registerFont (jam::fonts::DisplayBold_ttf, jam::fonts::DisplayBold_ttfSize);
    registerFont (jam::fonts::DisplayBook_ttf, jam::fonts::DisplayBook_ttfSize);
    registerFont (jam::fonts::DisplayMedium_ttf, jam::fonts::DisplayMedium_ttfSize);
    registerFont (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);
    registerFont (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);
    registerFont (jam::fonts::DisplayMonoMedium_ttf, jam::fonts::DisplayMonoMedium_ttfSize);

    // Applies the shipped/user-configured rasterization backend and coverage
    // LUT gamma/contrast before this atlas ever paints a glyph.
    setFontRasterization();
    setEmbolden();
}

void ENDLookAndFeel::setFontRasterization()
{
    auto* atlas { jam::GlyphAtlas::getInstance() };
    jassert (atlas != nullptr);

    const auto backendName { config.getValue (Id::toType (Id::graphics), Id::fontRasterizer).toString() };
    const auto backend { static_cast<map::FontRasterizerBackend::value> (map::FontRasterizerBackend::getInstance()->get (backendName)) };
    const float gamma { config.getValue (Id::toType (Id::graphics), Id::fontGamma) };
    const float contrast { config.getValue (Id::toType (Id::graphics), Id::fontContrast) };

    atlas->setRasterization (backend, gamma, contrast);

    // Rebuilding the atlas invalidates every cached glyph bitmap for an
    // otherwise-unchanged GlyphAtlas::Key (backend/gamma/contrast changed,
    // typeface/glyphIndex/fontSize did not) — EditorView's own
    // lookAndFeelChanged() (via ENDLookAndFeel::getCodeMetrics()) is the
    // sole path that recomputes cell metrics against the now-current atlas
    // state and repaints every pane. ENDLookAndFeel owns no Component of
    // its own (unlike ENDView's own theme callback, which fires
    // Component::sendLookAndFeelChange() directly — EventRegistration.cpp,
    // end/), so the SAME call is reached here through juce::Desktop's
    // top-level window registry instead. A no-op at startup
    // (registerTypeface()'s own tail call runs before jam::Window exists —
    // ENDApplication::initialiseVulkan() precedes jam::Window construction,
    // Main.cpp — so juce::Desktop holds zero top-level components then).
    cascadeLookAndFeelChange();
}

void ENDLookAndFeel::setEmbolden()
{
    auto* atlas { jam::GlyphAtlas::getInstance() };
    jassert (atlas != nullptr);

    const bool embolden { config.getValue (Id::toType (Id::code), Id::embolden) };

    atlas->setEmbolden (embolden);

    // Same atlas-invalidation cascade as setFontRasterization()'s own tail
    // comment — embolden changes the rasterized bitmap for an
    // otherwise-unchanged GlyphAtlas::Key too.
    cascadeLookAndFeelChange();
}

void ENDLookAndFeel::initialiseColours()
{
    colourScheme = jam::ColourScheme::fromValueTree (config.state);

    colourScheme.addColourId (Id::toType (Id::code), Id::caret, juce::CaretComponent::caretColourId);
    colourScheme.addColourId (Id::toType (Id::code), Id::selectionCursor, selectionCursorColourId);
    colourScheme.addColourId (Id::toType (Id::code), Id::editorBackground, juce::TextEditor::backgroundColourId);
    colourScheme.addColourId (Id::toType (Id::code), Id::editorOutline, juce::TextEditor::outlineColourId);
    colourScheme.addColourId (Id::toType (Id::scrollbar), Id::thumb, juce::ScrollBar::thumbColourId);
    colourScheme.addColourId (Id::toType (Id::scrollbar), Id::track, juce::ScrollBar::trackColourId);
    colourScheme.addColourId (Id::toType (Id::tab), Id::background, jam::ButtonBar::backgroundColourId);
    colourScheme.addColourId (Id::toType (Id::tab), Id::highlight, jam::ButtonBar::highlightColourId);
    colourScheme.addColourId (Id::toType (Id::tab), Id::outline, jam::ButtonBar::outlineColourId);
    colourScheme.addColourId (Id::toType (Id::button), Id::button, juce::TextButton::buttonColourId);
    colourScheme.addColourId (Id::toType (Id::button), Id::buttonOn, juce::TextButton::buttonOnColourId);
    colourScheme.addColourId (Id::toType (Id::button), Id::textOff, juce::TextButton::textColourOffId);
    colourScheme.addColourId (Id::toType (Id::button), Id::textOn, juce::TextButton::textColourOnId);
    colourScheme.addColourId (Id::toType (Id::overlay), Id::background, juce::Label::backgroundColourId);
    colourScheme.addColourId (Id::toType (Id::overlay), Id::text, juce::Label::textColourId);
    colourScheme.addColourId (Id::toType (Id::pane), Id::resizeBar, paneBarColourId);
    colourScheme.addColourId (Id::toType (Id::pane), Id::resizeBarHighlight, paneBarHighlightColourId);
    colourScheme.addColourId (Id::toType (Id::pane), Id::outline, jam::PaneComponent::outlineColourId);
    colourScheme.addColourId (Id::toType (Id::pane), Id::focusedOutline, jam::PaneComponent::focusedOutlineColourId);

    colourScheme.applyColours (*this, config.state);

    setPopupMenuColours();
}

void ENDLookAndFeel::setPopupMenuColours()
{
    const auto windowColour { jam::ColourScheme::toColour (config.getValue (Id::toType (Id::window), Id::background)) };
    const float menuOpacity { config.getValue (Id::toType (Id::menu), Id::opacity) };
    const auto textColour { jam::ColourScheme::toColour (config.getValue (Id::toType (Id::menu), Id::text)) };
    const auto highlightColour { jam::ColourScheme::toColour (config.getValue (Id::toType (Id::menu), Id::highlight)) };

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

    const auto applyColours = [this] (juce::ValueTree&)
    {
        colourScheme.applyColours (*this, config.state);
    };

    events.add<juce::ValueTree&> (Id::toType (Id::code), applyColours);
    events.add<juce::ValueTree&> (Id::toType (Id::scrollbar), applyColours);
    events.add<juce::ValueTree&> (Id::toType (Id::tab), applyColours);
    events.add<juce::ValueTree&> (Id::toType (Id::button), applyColours);
    events.add<juce::ValueTree&> (Id::toType (Id::overlay), applyColours);
    events.add<juce::ValueTree&> (Id::toType (Id::pane), applyColours);

    events.add<juce::ValueTree&> (Id::toType (Id::menu),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPopupMenuColours();
                                  });

    // Font-identity config coverage — fontRasterizer/fontGamma/fontContrast are
    // the ONLY display.md values requiring a route to setFontRasterization().
    // See this method's own doc comment for the full glyph-identity audit.
    events.add<juce::ValueTree&> (Id::fontRasterizer,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (Id::fontGamma,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (Id::fontContrast,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    // code.embolden — same font-owner precedent as fontRasterizer/fontGamma/
    // fontContrast.
    events.add<juce::ValueTree&> (Id::embolden,
                                  [this] (juce::ValueTree&)
                                  {
                                      setEmbolden();
                                  });
}
