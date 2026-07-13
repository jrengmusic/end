#include "ENDLookAndFeel.h"
#include "Bimap.h"

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

    const auto backendName { config.getValue (IDtype::graphics, ID::fontRasterizer).toString() };
    const auto backend { static_cast<jam::GlyphAtlas::Backend> (FontRasterizerBackend::get (backendName)) };
    const float gamma { config.getValue (IDtype::graphics, ID::fontGamma) };
    const float contrast { config.getValue (IDtype::graphics, ID::fontContrast) };

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

    const bool embolden { config.getValue (IDtype::code, ID::embolden) };

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

    setColourId (IDtype::code, jam::ID::text, jam::CodeView::textColourId);
    setColourId (IDtype::code, jam::ID::background, jam::CodeView::backgroundColourId);
    setColourId (IDtype::code, ID::caret, juce::CaretComponent::caretColourId);
    setColourId (IDtype::code, ID::highlight, jam::CodeView::selectionColourId);
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
    setColourId (IDtype::pane, jam::ID::outline, jam::PaneComponent::outlineColourId);
    setColourId (IDtype::pane, ID::focusedOutline, jam::PaneComponent::focusedOutlineColourId);
    setColourId (IDtype::statusBar, jam::ID::background, statusBarBackgroundColourId);
    setColourId (IDtype::statusBar, ID::labelBackground, statusBarLabelBackgroundColourId);
    setColourId (IDtype::statusBar, ID::labelText, statusBarLabelTextColourId);
    setColourId (IDtype::statusBar, ID::spinner, statusBarSpinnerColourId);
    setColourId (IDtype::hint, jam::ID::background, hintLabelBgColourId);
    setColourId (IDtype::hint, jam::ID::text, hintLabelFgColourId);

    setColours (config.state);
    setPopupMenuColours();
}

void ENDLookAndFeel::setPopupMenuColours()
{
    const auto windowColour { jam::Model::toColour (config.getValue (IDtype::window, jam::ID::background)) };
    const float menuOpacity { config.getValue (IDtype::menu, ID::opacity) };
    const auto textColour { jam::Model::toColour (config.getValue (IDtype::menu, jam::ID::text)) };
    const auto highlightColour { jam::Model::toColour (config.getValue (IDtype::menu, ID::highlight)) };

    setColour (juce::PopupMenu::backgroundColourId, windowColour.withAlpha (menuOpacity));
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, highlightColour);
    setColour (juce::PopupMenu::highlightedTextColourId, textColour);
}

void ENDLookAndFeel::loadGraphics()
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

void ENDLookAndFeel::registerEvents()
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

    events.add<juce::ValueTree&> (IDtype::menu,
                                  [this] (juce::ValueTree&)
                                  {
                                      setPopupMenuColours();
                                  });

    // Font-identity config coverage — fontRasterizer/fontGamma/fontContrast are
    // the ONLY config.lua values requiring a route to setFontRasterization().
    // See this method's own doc comment for the full glyph-identity audit.
    events.add<juce::ValueTree&> (ID::fontRasterizer,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (ID::fontGamma,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    events.add<juce::ValueTree&> (ID::fontContrast,
                                  [this] (juce::ValueTree&)
                                  {
                                      setFontRasterization();
                                  });

    // code.embolden — same font-owner precedent as fontRasterizer/fontGamma/
    // fontContrast.
    events.add<juce::ValueTree&> (ID::embolden,
                                  [this] (juce::ValueTree&)
                                  {
                                      setEmbolden();
                                  });
}
