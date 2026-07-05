#include "terminal/View.h"
#include "Bimap.h"

namespace terminal
{
/*____________________________________________________________________________*/

void View::registerEvents()
{
    // code.ligatures — CodeView shaping-pass toggle (jam::GlyphArrangement::
    // tryLigature). Property key — one specific config value, one handler.
    events.add<juce::ValueTree&> (ID::ligatures,
                                  [this] (juce::ValueTree&)
                                  {
                                      codeView->setLigatures (config.getValue (IDtype::code, ID::ligatures));
                                  });

    // Cursor block (style/blink/blink_interval/char/force) — tree-type key,
    // re-applies the whole block on any property change within it, mirroring
    // end::LookAndFeel's per-tree-type colour refresh (setColours()) precedent.
    events.add<juce::ValueTree&> (jam::IDtype::cursor,
                                  [this] (juce::ValueTree&)
                                  {
                                      applyCursorConfig();
                                  });
}

void View::applyCursorConfig()
{
    const auto styleName { config.getValue (jam::IDtype::cursor, jam::ID::style).toString() };

    if (end::CursorShape::getInstance()->contains (styleName))
        codeView->setCaretShape (static_cast<jam::CaretShape> (end::CursorShape::get (styleName)));

    const bool blink { config.getValue (jam::IDtype::cursor, ID::blink) };
    codeView->setCaretBlink (blink);
    codeView->setCaretBlinkInterval (config.getValue (jam::IDtype::cursor, ID::blinkInterval));

    // PLUMB only — glyph-char caret rendering is flagged, not built (the
    // emoji/glyph-caret bundle). Stored for the DECSCUSR gate to consume.
    cursorChar  = config.getValue (jam::IDtype::cursor, ID::cursorChar).toString();
    cursorForce = config.getValue (jam::IDtype::cursor, ID::force);
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
