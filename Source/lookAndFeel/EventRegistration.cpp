#include "ENDLookAndFeel.h"

void ENDLookAndFeel::registerTypeface (jam::GlyphAtlas& atlas)
{
    // One pass, one parse per font (SSOT font list) — creates the Ptr, stores
    // it in typefaces under its {name, style} composite key (typefaceKey()) for
    // name+style lookup (getTypefaceForFont()), and registers the SAME Ptr
    // identity with the atlas, all in a single lambda call per font instead of
    // two separate passes re-parsing the same bytes.
    auto add = [this, &atlas] (const void* data, int size)
    {
        auto ptr { juce::Typeface::createSystemTypefaceFor (data, size) };
        auto key { typefaceKey (ptr->getName(), ptr->getStyle()) };
        typefaces.addOrReplace (key, ptr);
        atlas.registerTypeface (ptr, data, static_cast<size_t> (size));

        // jam::Typeface (hb_font_t interning for jam::GlyphArrangement's
        // cmap lookup + tryLigature() shaping) is
        // registered from the SAME (ptr, data, size) triple, one pass.
        jam::Typeface::getInstance()->registerTypeface (ptr, data, static_cast<size_t> (size));
    };

    add (jam::fonts::DisplayBold_ttf, jam::fonts::DisplayBold_ttfSize);
    add (jam::fonts::DisplayBook_ttf, jam::fonts::DisplayBook_ttfSize);
    add (jam::fonts::DisplayMedium_ttf, jam::fonts::DisplayMedium_ttfSize);
    add (jam::fonts::DisplayMonoBold_ttf, jam::fonts::DisplayMonoBold_ttfSize);
    add (jam::fonts::DisplayMonoBook_ttf, jam::fonts::DisplayMonoBook_ttfSize);
    add (jam::fonts::DisplayMonoMedium_ttf, jam::fonts::DisplayMonoMedium_ttfSize);

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
    const auto backend { static_cast<jam::GlyphAtlas::Backend> (Id::FontRasterizerBackend::get (backendName)) };
    const float gamma { config.getValue (Id::toType (Id::graphics), Id::fontGamma) };
    const float contrast { config.getValue (Id::toType (Id::graphics), Id::fontContrast) };

    atlas->setRasterization (backend, gamma, contrast);

    // Rebuilding the atlas invalidates every cached glyph bitmap for an
    // otherwise-unchanged GlyphAtlas::Key (backend/gamma/contrast changed,
    // typeface/glyphIndex/fontSize did not) — EditorView's own
    // lookAndFeelChanged() (via ENDLookAndFeel::getCodeMetrics()) is the
    // sole path that recomputes cell metrics against the now-current atlas
    // state and repaints every pane. ENDLookAndFeel owns no Component of
    // its own (unlike ENDView's own theme handler, which fires
    // Component::sendLookAndFeelChange() directly — EventRegistration.cpp,
    // end/), so the SAME call is reached here through juce::Desktop's
    // top-level window registry instead. A no-op at startup
    // (registerTypeface()'s own tail call runs before ENDWindow exists —
    // ENDApplication::initialiseVulkan() precedes the window.reset() call,
    // Main.cpp — so juce::Desktop holds zero top-level components then).
    auto& desktop { juce::Desktop::getInstance() };

    for (int i = 0; i < desktop.getNumComponents(); ++i)
        desktop.getComponent (i)->sendLookAndFeelChange();
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
    auto& desktop { juce::Desktop::getInstance() };

    for (int i = 0; i < desktop.getNumComponents(); ++i)
        desktop.getComponent (i)->sendLookAndFeelChange();
}

void ENDLookAndFeel::initialiseColours()
{
    colourMap = jam::ColourMap::fromValueTree (config.state);

    setColourId (Id::toType (Id::code), Id::text, jam::CodeView::textColourId);
    setColourId (Id::toType (Id::code), Id::background, jam::CodeView::backgroundColourId);
    setColourId (Id::toType (Id::code), Id::caret, juce::CaretComponent::caretColourId);
    setColourId (Id::toType (Id::code), Id::highlight, jam::CodeView::selectionColourId);
    setColourId (Id::toType (Id::code), Id::selectionCursor, selectionCursorColourId);
    setColourId (Id::toType (Id::code), Id::editorBackground, juce::TextEditor::backgroundColourId);
    setColourId (Id::toType (Id::code), Id::editorOutline, juce::TextEditor::outlineColourId);
    setColourId (Id::toType (Id::scrollbar), Id::thumb, juce::ScrollBar::thumbColourId);
    setColourId (Id::toType (Id::scrollbar), Id::track, juce::ScrollBar::trackColourId);
    setColourId (Id::toType (Id::tab), Id::background, jam::ButtonBar::backgroundColourId);
    setColourId (Id::toType (Id::tab), Id::highlight, jam::ButtonBar::highlightColourId);
    setColourId (Id::toType (Id::tab), Id::outline, jam::ButtonBar::outlineColourId);
    setColourId (Id::toType (Id::button), Id::button, juce::TextButton::buttonColourId);
    setColourId (Id::toType (Id::button), Id::buttonOn, juce::TextButton::buttonOnColourId);
    setColourId (Id::toType (Id::button), Id::textOff, juce::TextButton::textColourOffId);
    setColourId (Id::toType (Id::button), Id::textOn, juce::TextButton::textColourOnId);
    setColourId (Id::toType (Id::overlay), Id::background, juce::Label::backgroundColourId);
    setColourId (Id::toType (Id::overlay), Id::text, juce::Label::textColourId);
    setColourId (Id::toType (Id::pane), Id::resizeBar, paneBarColourId);
    setColourId (Id::toType (Id::pane), Id::resizeBarHighlight, paneBarHighlightColourId);
    setColourId (Id::toType (Id::pane), Id::outline, jam::PaneComponent::outlineColourId);
    setColourId (Id::toType (Id::pane), Id::focusedOutline, jam::PaneComponent::focusedOutlineColourId);
    setColourId (Id::toType (Id::statusBar), Id::background, statusBarBackgroundColourId);
    setColourId (Id::toType (Id::statusBar), Id::labelBackground, statusBarLabelBackgroundColourId);
    setColourId (Id::toType (Id::statusBar), Id::labelText, statusBarLabelTextColourId);
    setColourId (Id::toType (Id::statusBar), Id::spinner, statusBarSpinnerColourId);
    setColourId (Id::toType (Id::hint), Id::background, hintLabelBgColourId);
    setColourId (Id::toType (Id::hint), Id::text, hintLabelFgColourId);

    setColours (config.state);
    setPopupMenuColours();
}

void ENDLookAndFeel::setPopupMenuColours()
{
    const auto windowColour { jam::Model::toColour (config.getValue (Id::toType (Id::window), Id::background)) };
    const float menuOpacity { config.getValue (Id::toType (Id::menu), Id::opacity) };
    const auto textColour { jam::Model::toColour (config.getValue (Id::toType (Id::menu), Id::text)) };
    const auto highlightColour { jam::Model::toColour (config.getValue (Id::toType (Id::menu), Id::highlight)) };

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
                {
                    auto stem { propName.toString() };
                    auto suffix { stem.fromLastOccurrenceOf ("_", false, false) };

                    juce::Identifier id { suffix.isNotEmpty()
                        and ::Id::ButtonState::getInstance()->contains (suffix)
                            ? suffix : stem };

                    graphics.addOrReplace (id,
                        jam::SVG::Flex::getSegments (value.toString(), colourMap));
                }
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

    events.add<juce::ValueTree&> (Id::toType (Id::code),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::scrollbar),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::tab),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::button),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::overlay),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::pane),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::statusBar),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::hint),
                                  [this] (juce::ValueTree&)
                                  {
                                      setColours (config.state);
                                  });

    events.add<juce::ValueTree&> (Id::toType (Id::menu),
                                  [this] (juce::ValueTree&)
                                  {
                                      setPopupMenuColours();
                                  });

    // Font-identity config coverage — fontRasterizer/fontGamma/fontContrast are
    // the ONLY config.lua values requiring a route to setFontRasterization().
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
