#include "terminal/View.h"

namespace terminal
{
/*____________________________________________________________________________*/

View::View (jam::UUID uuid, jam::Model& model, Session& sessionRef)
    : end::PaneView (uuid, model)
    , session (sessionRef)
{
    auto fontFamily { config.getValue (IDtype::code, ID::fontFamily) };
    const float fontSize { config.getValue (IDtype::code, ID::fontSize) };
    const float cellWidthRatio { config.getValue (IDtype::code, ID::cellWidth) };
    const float lineHeightRatio { config.getValue (IDtype::code, ID::lineHeight) };

    const juce::Font font { juce::FontOptions {}.withName (fontFamily).withPointHeight (fontSize) };

    auto resolvedTypeface { font.getTypefacePtr() };

    // endless conformance restoration (jam::GlyphAtlas::calcMetrics(), commit
    // 2e37f6d) — cell metrics come from the FT face's own advance/ascender/
    // height at the exact size rasterize() sizes it to, rather than JUCE's
    // juce::GlyphArrangement::getStringWidth()/getAscent() estimate.
    auto* atlas { jam::GlyphAtlas::getInstance() };
    jassert (atlas != nullptr);
    const auto metrics { atlas->calcMetrics (resolvedTypeface, font.getHeight()) };

    cellWidthPx  = juce::roundToInt (static_cast<float> (metrics.cellWidth) * cellWidthRatio);
    cellHeightPx = juce::roundToInt (static_cast<float> (metrics.cellHeight) * lineHeightRatio);

    // Direction B (RFC P12) — cellSize + scrollbackLines published once at
    // construction; winsize is published continuously from resized() (S4).
    session.getModel().setCellSize (jam::Size<int16_t> (cellWidthPx, cellHeightPx));
    session.getModel().setScrollbackLines (config.getValue (IDtype::terminal, ID::scrollbackLines));

    auto [top, right, bottom, left] = config.getInt16 (IDtype::code, jam::ID::padding);
    textPadding = juce::BorderSize<int> { top, left, bottom, right };

    gutterWidth = config.getValue (IDtype::scrollbar, jam::ID::width);

    codeView = std::make_unique<jam::CodeView> (session.getDocument());
    addAndMakeVisible (*codeView);
    codeView->setFont (font);
    codeView->setCellSize (cellWidthPx, cellHeightPx);

    // FT baseline wins over setFont()'s juce-ascent default (see setBaseline()'s
    // doc comment) — must run AFTER setFont()/setCellSize() so this call is the
    // one that sticks.
    codeView->setBaseline (metrics.baseline);
    codeView->setLigatures (config.getValue (IDtype::code, ID::ligatures));

    applyCursorConfig();
    registerEvents();
    config.addListener (this);

    // HARNESS (S7.1) — remove at PLAN Step 6.
    seedHarnessContent();
}

View::~View() { config.removeListener (this); }

void View::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    auto key { events.contains (property) ? property : tree.getType() };

    if (events.contains (key))
        events.get (key, tree);
}

void View::resized()
{
    const auto content { textPadding.subtractedFrom (getLocalBounds()) };
    const int cols { (content.getWidth() - gutterWidth) / cellWidthPx };
    const int rows { content.getHeight() / cellHeightPx };

    codeView->setBounds (content);
    codeView->setViewportWidth (cols);
    codeView->setViewportLineCount (rows);

    session.getModel().setWinsize (jam::Size<int16_t> (cols, rows));
}

//==============================================================================
// HARNESS (S7.1) — remove at PLAN Step 6.
//
// Seeds session.getDocument() with content exercising the full validation
// gate (PLAN-terminal-editor.md Step 2 "Validation gate"): plain numbered
// lines (scroll position visually verifiable), long lines (wrap), styled
// runs (every SGR attribute + underline style 1-5), wide CJK + emoji
// graphemes, and a final prompt line.
/** @brief styleId 0 is the plain/default jam::Stamp::Entry — interned by
 *  Session's constructor (terminal/Session.h) before any View exists. */
static constexpr uint16_t plainStyleId { 0 };

/** @brief Builds a CodeLine from @p text, one jam::Char per Unicode scalar via
 *  jam::Char::fromCodepoint. fromCodepoint returns exactly one Char per
 *  codepoint — a wide (CJK) head cell never carries its own companion, so a
 *  SPACER_TAIL cell is appended here whenever the returned cell is WIDE. */
static jam::CodeLine buildLine (const juce::String& text, uint16_t styleId)
{
    std::vector<jam::Char> cells;
    auto characterPointer { text.getCharPointer() };

    while (not characterPointer.isEmpty())
    {
        const auto codepoint { static_cast<uint32_t> (characterPointer.getAndAdvance()) };
        const auto headCell { jam::Char::fromCodepoint (codepoint, styleId, false) };

        cells.push_back (headCell);

        if (headCell.wide() == jam::Char::WIDE)
            cells.push_back (
                jam::Char::make (0, jam::Char::CONTENT_CODEPOINT, jam::Char::SPACER_TAIL, styleId));
    }

    jam::CodeLine line;
    line.cellCount = static_cast<int> (cells.size());
    line.chars.allocate (line.cellCount, false);

    for (int index { 0 }; index < line.cellCount; ++index)
        line.chars[index] = cells.at (static_cast<size_t> (index));

    return line;
}

/** @brief Builds a juce::String from explicit Unicode scalars — sidesteps
 *  source-file encoding entirely (CJK demonstration codepoints are exact
 *  hex values, never literal multi-byte source text). */
static juce::String
textFromCodepoints (const juce::String& prefix, std::initializer_list<char32_t> codepoints)
{
    juce::String text { prefix };

    for (auto codepoint : codepoints)
        text += juce::String::charToString (static_cast<juce::juce_wchar> (codepoint));

    return text;
}

/** @brief Builds a CodeLine whose final two cells are one emoji grapheme
 *  cluster (WIDE head + SPACER_TAIL companion), preceded by @p prefix in
 *  plain (styleId 0) text. Demonstrates jam::Grapheme interning + wide-cell
 *  companion pairing outside jam::Char::fromCodepoint's single-codepoint
 *  contract (fromCodepoint never produces CONTENT_GRAPHEME cells). */
static jam::CodeLine
buildEmojiLine (const juce::String& prefix, char32_t emojiCodepoint, uint16_t styleId)
{
    const auto prefixLine { buildLine (prefix, plainStyleId) };

    jam::Grapheme::Entry entry;
    entry.codepoints.at (0) = emojiCodepoint;
    entry.count = 1;

    const auto graphemeIndex { jam::Grapheme::getInstance()->addIfNotAlreadyThere (entry) };

    jam::CodeLine line;
    line.cellCount = prefixLine.cellCount + 2;
    line.chars.allocate (line.cellCount, false);

    for (int index { 0 }; index < prefixLine.cellCount; ++index)
        line.chars[index] = prefixLine.chars[index];

    line.chars[prefixLine.cellCount] = jam::Char::make (static_cast<uint32_t> (graphemeIndex),
                                                        jam::Char::CONTENT_GRAPHEME,
                                                        jam::Char::WIDE,
                                                        styleId);
    line.chars[prefixLine.cellCount + 1] =
        jam::Char::make (0, jam::Char::CONTENT_CODEPOINT, jam::Char::SPACER_TAIL, styleId);

    return line;
}

void View::seedHarnessContent()
{
    auto& document { session.getDocument() };
    auto* stamp { jam::Stamp::getInstance() };

    static constexpr int plainLineCount { 200 };
    static constexpr int wrapExerciseRepeatCount { 12 };

    for (int index { 0 }; index < plainLineCount; ++index)
        document.append (buildLine (
            juce::String::formatted ("%04d  plain scroll-verification line", index), plainStyleId));

    // Long lines (> 200 cells) — exercise wrap.
    const auto longLine { juce::String::repeatedString (
        "wrap-exercise-cell ", wrapExerciseRepeatCount) };
    document.append (buildLine (longLine, plainStyleId));
    document.append (buildLine (longLine.toUpperCase(), plainStyleId));

    // Styled runs — colored fg/bg.
    const auto colouredStyle { stamp->addIfNotAlreadyThere (
        jam::Stamp::Entry { juce::Colours::black, juce::Colours::yellow, juce::Colour {}, 0 }) };
    document.append (
        buildLine ("colored fg/bg: black on yellow", static_cast<uint16_t> (colouredStyle)));

    const auto boldStyle { stamp->addIfNotAlreadyThere (
        jam::Stamp::Entry { juce::Colours::white,
                            juce::Colours::transparentBlack,
                            juce::Colour {},
                            jam::Stamp::BOLD }) };
    document.append (buildLine ("BOLD sample text", static_cast<uint16_t> (boldStyle)));

    const auto italicStyle { stamp->addIfNotAlreadyThere (
        jam::Stamp::Entry { juce::Colours::white,
                            juce::Colours::transparentBlack,
                            juce::Colour {},
                            jam::Stamp::ITALIC }) };
    document.append (buildLine ("ITALIC sample text", static_cast<uint16_t> (italicStyle)));

    const auto dimStyle { stamp->addIfNotAlreadyThere (jam::Stamp::Entry {
        juce::Colours::white, juce::Colours::transparentBlack, juce::Colour {}, jam::Stamp::DIM }) };
    document.append (buildLine ("DIM sample text", static_cast<uint16_t> (dimStyle)));

    const auto inverseStyle { stamp->addIfNotAlreadyThere (jam::Stamp::Entry {
        juce::Colours::cyan, juce::Colours::black, juce::Colour {}, jam::Stamp::INVERSE }) };
    document.append (buildLine ("INVERSE sample text", static_cast<uint16_t> (inverseStyle)));

    const auto strikeStyle { stamp->addIfNotAlreadyThere (
        jam::Stamp::Entry { juce::Colours::white,
                            juce::Colours::transparentBlack,
                            juce::Colour {},
                            jam::Stamp::STRIKE }) };
    document.append (buildLine ("STRIKE sample text", static_cast<uint16_t> (strikeStyle)));

    const auto overlineStyle { stamp->addIfNotAlreadyThere (
        jam::Stamp::Entry { juce::Colours::white,
                            juce::Colours::transparentBlack,
                            juce::Colour {},
                            jam::Stamp::OVERLINE }) };
    document.append (buildLine ("OVERLINE sample text", static_cast<uint16_t> (overlineStyle)));

    static constexpr std::array<uint16_t, 5> underlineStyles { jam::Stamp::UNDERLINE_SINGLE,
                                                               jam::Stamp::UNDERLINE_DOUBLE,
                                                               jam::Stamp::UNDERLINE_CURLY,
                                                               jam::Stamp::UNDERLINE_DOTTED,
                                                               jam::Stamp::UNDERLINE_DASHED };

    for (int index { 0 }; index < static_cast<int> (underlineStyles.size()); ++index)
    {
        const auto underlineStyle { stamp->addIfNotAlreadyThere (
            jam::Stamp::Entry { juce::Colours::white,
                                juce::Colours::transparentBlack,
                                juce::Colour {},
                                underlineStyles.at (static_cast<size_t> (index)) }) };
        document.append (buildLine ("UNDERLINE style " + juce::String (index + 1) + " sample text",
                                    static_cast<uint16_t> (underlineStyle)));
    }

    // Wide CJK characters — jam::Char::fromCodepoint auto-detects width.
    // Codepoints: U+4E2D U+6587 U+5B57 U+7B26 U+6D4B U+8BD5 ("中文字符测试").
    static constexpr std::initializer_list<char32_t> harnessCjkCodepoints {
        0x4E2D, 0x6587, 0x5B57, 0x7B26, 0x6D4B, 0x8BD5
    };
    document.append (
        buildLine (textFromCodepoints ("CJK width test: ", harnessCjkCodepoints), plainStyleId));

    // Emoji grapheme cluster — jam::Grapheme interning + wide-cell companion pairing.
    // Codepoint: U+1F600 (GRINNING FACE).
    static constexpr char32_t harnessEmojiCodepoint { 0x1F600 };
    document.append (buildEmojiLine ("emoji grapheme test: ", harnessEmojiCodepoint, plainStyleId));

    // Final prompt line.
    document.append (buildLine ("$ ", plainStyleId));

    // caretRow is viewport-relative (jam_CodeView.h) — row 0 of the eventual
    // live region. Real cursor-anchored placement lands with terminal::Model
    // (Step 5); this harness only exercises the setCaretPosition code path.
    codeView->setCaretPosition (0, 2);
    codeView->setCaretShape (jam::CaretShape::block);
    codeView->calc();
    codeView->scrollToBottom();
}

/**______________________________END OF NAMESPACE______________________________*/
}// namespace terminal
