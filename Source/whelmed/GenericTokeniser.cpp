#include "GenericTokeniser.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

static constexpr int tokenError        { 0 };
static constexpr int tokenComment      { 1 };
static constexpr int tokenKeyword      { 2 };
static constexpr int tokenOperator     { 3 };
static constexpr int tokenIdentifier   { 4 };
static constexpr int tokenInteger      { 5 };
static constexpr int tokenFloat        { 6 };
static constexpr int tokenString       { 7 };
static constexpr int tokenBracket      { 8 };
static constexpr int tokenPunctuation  { 9 };
static constexpr int tokenPreprocessor { 10 };

//==============================================================================
/** Peeks ahead in source to check if the next N chars match pattern.
    Does NOT consume any characters. */
static bool matchesAhead (juce::CodeDocument::Iterator source,
                          const juce::String& pattern)
{
    bool matched { true };

    for (int i { 0 }; i < pattern.length() and matched; ++i)
    {
        const juce::juce_wchar ch { source.nextChar() };
        matched = (ch == pattern[i]);
    }

    return matched;
}

/** Consumes chars from source until the pattern is found (inclusive), or EOF.
    The closing pattern itself is consumed. */
static void consumeUntil (juce::CodeDocument::Iterator& source,
                          const juce::String& endPattern)
{
    for (;;)
    {
        if (source.isEOF())
            break;

        if (matchesAhead (source, endPattern))
        {
            for (int i { 0 }; i < endPattern.length(); ++i)
                source.nextChar();

            break;
        }

        source.nextChar();
    }
}

//==============================================================================
GenericTokeniser::GenericTokeniser (const LanguageDefinition& definition)
    : lang (definition),
      sortedKeywords (definition.keywords)
{
    sortedKeywords.sort (false);
}

//==============================================================================
int GenericTokeniser::readNextToken (juce::CodeDocument::Iterator& source)
{
    if (source.isEOF())
        return tokenError;

    const juce::juce_wchar first { source.peekNextChar() };

    // Whitespace — consume and return tokenError (matches JUCE convention)
    if (juce::CharacterFunctions::isWhitespace (first))
    {
        source.nextChar();
        return tokenError;
    }

    // Block comment
    if (lang.blockCommentStart.isNotEmpty() and matchesAhead (source, lang.blockCommentStart))
    {
        for (int i { 0 }; i < lang.blockCommentStart.length(); ++i)
            source.nextChar();

        consumeUntil (source, lang.blockCommentEnd);
        return tokenComment;
    }

    // Line comment
    if (lang.lineComment.isNotEmpty() and matchesAhead (source, lang.lineComment))
    {
        for (int i { 0 }; i < lang.lineComment.length(); ++i)
            source.nextChar();

        while (not source.isEOF() and source.peekNextChar() != '\n')
            source.nextChar();

        return tokenComment;
    }

    // String — double quote
    if (first == '"')
    {
        source.nextChar();

        while (not source.isEOF())
        {
            const juce::juce_wchar ch { source.nextChar() };

            if (ch == '\\')
            {
                if (not source.isEOF())
                    source.nextChar();
            }
            else if (ch == '"')
            {
                break;
            }
        }

        return tokenString;
    }

    // String — single quote
    if (first == '\'')
    {
        source.nextChar();

        while (not source.isEOF())
        {
            const juce::juce_wchar ch { source.nextChar() };

            if (ch == '\\')
            {
                if (not source.isEOF())
                    source.nextChar();
            }
            else if (ch == '\'')
            {
                break;
            }
        }

        return tokenString;
    }

    // String — backtick (only if language enables it)
    if (lang.hasBacktickStrings and first == '`')
    {
        source.nextChar();

        while (not source.isEOF())
        {
            const juce::juce_wchar ch { source.nextChar() };

            if (ch == '\\')
            {
                if (not source.isEOF())
                    source.nextChar();
            }
            else if (ch == '`')
            {
                break;
            }
        }

        return tokenString;
    }

    // Number — digit, or '.' followed by a digit
    const bool startsWithDot { first == '.'
        and not source.isEOF()
        and [&]()
            {
                juce::CodeDocument::Iterator peek { source };
                peek.nextChar(); // consume '.'
                return juce::CharacterFunctions::isDigit (peek.peekNextChar());
            }() };

    if (juce::CharacterFunctions::isDigit (first) or startsWithDot)
    {
        bool isFloat { startsWithDot };
        bool isHex   { false };

        source.nextChar(); // consume first char

        // Hex prefix
        if (first == '0' and not source.isEOF())
        {
            const juce::juce_wchar next { source.peekNextChar() };

            if (next == 'x' or next == 'X')
            {
                source.nextChar();
                isHex = true;

                while (not source.isEOF()
                       and juce::CppTokeniserFunctions::isHexDigit (source.peekNextChar()))
                {
                    source.nextChar();
                }
            }
        }

        if (not isHex)
        {
            while (not source.isEOF() and juce::CharacterFunctions::isDigit (source.peekNextChar()))
                source.nextChar();

            // Fractional part
            if (not isFloat and not source.isEOF() and source.peekNextChar() == '.')
            {
                juce::CodeDocument::Iterator peek { source };
                peek.nextChar(); // skip '.'
                const bool digitAfterDot { juce::CharacterFunctions::isDigit (peek.peekNextChar()) };

                if (digitAfterDot)
                {
                    source.nextChar(); // consume '.'
                    isFloat = true;

                    while (not source.isEOF()
                           and juce::CharacterFunctions::isDigit (source.peekNextChar()))
                    {
                        source.nextChar();
                    }
                }
            }

            // Exponent
            if (not source.isEOF())
            {
                const juce::juce_wchar expChar { source.peekNextChar() };

                if (expChar == 'e' or expChar == 'E')
                {
                    source.nextChar();
                    isFloat = true;

                    if (not source.isEOF())
                    {
                        const juce::juce_wchar sign { source.peekNextChar() };

                        if (sign == '+' or sign == '-')
                            source.nextChar();
                    }

                    while (not source.isEOF()
                           and juce::CharacterFunctions::isDigit (source.peekNextChar()))
                    {
                        source.nextChar();
                    }
                }
            }

            // Float suffix
            if (not source.isEOF())
            {
                const juce::juce_wchar suffix { source.peekNextChar() };

                if (suffix == 'f' or suffix == 'F')
                {
                    source.nextChar();
                    isFloat = true;
                }
            }
        }

        return isFloat ? tokenFloat : tokenInteger;
    }

    // Identifier or keyword
    if (juce::CharacterFunctions::isLetter (first) or first == '_')
    {
        juce::String word;

        while (not source.isEOF())
        {
            const juce::juce_wchar ch { source.peekNextChar() };

            if (juce::CharacterFunctions::isLetterOrDigit (ch) or ch == '_')
            {
                word += source.nextChar();
            }
            else
            {
                break;
            }
        }

        if (sortedKeywords.contains (word))
            return tokenKeyword;

        return tokenIdentifier;
    }

    // Bracket
    if (first == '(' or first == ')'
        or first == '[' or first == ']'
        or first == '{' or first == '}')
    {
        source.nextChar();
        return tokenBracket;
    }

    // Operator
    if (first == '=' or first == '+'  or first == '-' or first == '*'
        or first == '/' or first == '<' or first == '>' or first == '!'
        or first == '&' or first == '|' or first == '^' or first == '~'
        or first == '%' or first == '?' or first == ':')
    {
        source.nextChar();
        return tokenOperator;
    }

    // Anything else — punctuation
    source.nextChar();
    return tokenPunctuation;
}

//==============================================================================
juce::CodeEditorComponent::ColourScheme GenericTokeniser::getDefaultColourScheme()
{
    return {};
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
