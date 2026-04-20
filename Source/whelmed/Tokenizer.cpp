#include "Tokenizer.h"
#include "GenericTokeniser.h"
#include "../config/WhelmedConfig.h"

namespace Whelmed
{ /*____________________________________________________________________________*/

static LanguageDefinition getLanguageDefinition (const juce::String& language)
{
    LanguageDefinition def;

    if (language == "python" or language == "py")
    {
        def.lineComment = "#";
        def.blockCommentStart = "\"\"\"";
        def.blockCommentEnd = "\"\"\"";
        def.keywords = { "False", "None", "True", "and", "as", "assert", "async", "await",
                         "break", "class", "continue", "def", "del", "elif", "else", "except",
                         "finally", "for", "from", "global", "if", "import", "in", "is",
                         "lambda", "nonlocal", "not", "or", "pass", "raise", "return",
                         "try", "while", "with", "yield" };
    }
    else if (language == "javascript" or language == "js")
    {
        def.lineComment = "//";
        def.blockCommentStart = "/*";
        def.blockCommentEnd = "*/";
        def.hasBacktickStrings = true;
        def.keywords = { "async", "await", "break", "case", "catch", "class", "const",
                         "continue", "debugger", "default", "delete", "do", "else", "export",
                         "extends", "false", "finally", "for", "function", "if", "import",
                         "in", "instanceof", "let", "new", "null", "of", "return", "super",
                         "switch", "this", "throw", "true", "try", "typeof", "undefined",
                         "var", "void", "while", "with", "yield" };
    }
    else if (language == "typescript" or language == "ts")
    {
        def.lineComment = "//";
        def.blockCommentStart = "/*";
        def.blockCommentEnd = "*/";
        def.hasBacktickStrings = true;
        def.keywords = { "abstract", "any", "as", "async", "await", "boolean", "break",
                         "case", "catch", "class", "const", "constructor", "continue",
                         "debugger", "declare", "default", "delete", "do", "else", "enum",
                         "export", "extends", "false", "finally", "for", "from", "function",
                         "get", "if", "implements", "import", "in", "instanceof", "interface",
                         "is", "keyof", "let", "module", "namespace", "never", "new", "null",
                         "number", "object", "of", "package", "private", "protected", "public",
                         "readonly", "return", "require", "set", "static", "string", "super",
                         "switch", "symbol", "this", "throw", "true", "try", "type", "typeof",
                         "undefined", "unique", "unknown", "var", "void", "while", "with", "yield" };
    }
    else if (language == "rust" or language == "rs")
    {
        def.lineComment = "//";
        def.blockCommentStart = "/*";
        def.blockCommentEnd = "*/";
        def.keywords = { "as", "async", "await", "break", "const", "continue", "crate",
                         "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
                         "impl", "in", "let", "loop", "match", "mod", "move", "mut",
                         "pub", "ref", "return", "self", "Self", "static", "struct",
                         "super", "trait", "true", "type", "union", "unsafe", "use",
                         "where", "while" };
    }
    else if (language == "go")
    {
        def.lineComment = "//";
        def.blockCommentStart = "/*";
        def.blockCommentEnd = "*/";
        def.hasBacktickStrings = true;
        def.keywords = { "break", "case", "chan", "const", "continue", "default", "defer",
                         "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
                         "interface", "map", "package", "range", "return", "select", "struct",
                         "switch", "type", "var",
                         "bool", "byte", "complex64", "complex128", "error", "float32",
                         "float64", "int", "int8", "int16", "int32", "int64", "rune",
                         "string", "uint", "uint8", "uint16", "uint32", "uint64", "uintptr",
                         "true", "false", "nil", "iota", "append", "cap", "close", "copy",
                         "delete", "imag", "len", "make", "new", "panic", "print", "println",
                         "real", "recover" };
    }
    else if (language == "bash" or language == "sh" or language == "zsh")
    {
        def.lineComment = "#";
        def.keywords = { "case", "do", "done", "elif", "else", "esac", "fi", "for",
                         "function", "if", "in", "select", "then", "until", "while",
                         "break", "continue", "return", "exit",
                         "echo", "printf", "read", "declare", "local", "export",
                         "unset", "shift", "set", "eval", "exec", "source", "test",
                         "true", "false" };
    }
    else if (language == "sql")
    {
        def.lineComment = "--";
        def.blockCommentStart = "/*";
        def.blockCommentEnd = "*/";
        def.keywords = { "SELECT", "FROM", "WHERE", "INSERT", "INTO", "VALUES", "UPDATE",
                         "SET", "DELETE", "CREATE", "TABLE", "DROP", "ALTER", "INDEX",
                         "JOIN", "INNER", "LEFT", "RIGHT", "OUTER", "ON", "AND", "OR",
                         "NOT", "NULL", "IS", "IN", "BETWEEN", "LIKE", "ORDER", "BY",
                         "GROUP", "HAVING", "LIMIT", "OFFSET", "AS", "DISTINCT", "COUNT",
                         "SUM", "AVG", "MAX", "MIN", "UNION", "ALL", "EXISTS", "CASE",
                         "WHEN", "THEN", "ELSE", "END", "PRIMARY", "KEY", "FOREIGN",
                         "REFERENCES", "CONSTRAINT", "DEFAULT", "CHECK", "UNIQUE",
                         "INTEGER", "TEXT", "REAL", "BLOB", "VARCHAR", "BOOLEAN",
                         "select", "from", "where", "insert", "into", "values", "update",
                         "set", "delete", "create", "table", "drop", "alter", "index",
                         "join", "inner", "left", "right", "outer", "on", "and", "or",
                         "not", "null", "is", "in", "between", "like", "order", "by",
                         "group", "having", "limit", "offset", "as", "distinct", "count",
                         "sum", "avg", "max", "min", "union", "all", "exists", "case",
                         "when", "then", "else", "end", "primary", "key", "foreign",
                         "references", "constraint", "default", "check", "unique",
                         "integer", "text", "real", "blob", "varchar", "boolean" };
    }
    else if (language == "json")
    {
        def.keywords = { "true", "false", "null" };
    }
    else if (language == "yaml" or language == "yml")
    {
        def.lineComment = "#";
        def.keywords = { "true", "false", "null", "yes", "no", "on", "off" };
    }
    else if (language == "swift")
    {
        def.lineComment = "//";
        def.blockCommentStart = "/*";
        def.blockCommentEnd = "*/";
        def.keywords = { "actor", "any", "as", "associatedtype", "async", "await", "break",
                         "case", "catch", "class", "continue", "default", "defer", "deinit",
                         "do", "else", "enum", "extension", "fallthrough", "false", "fileprivate",
                         "for", "func", "guard", "if", "import", "in", "init", "inout",
                         "internal", "is", "lazy", "let", "nil", "open", "operator", "override",
                         "private", "protocol", "public", "repeat", "rethrows", "return",
                         "self", "Self", "some", "static", "struct", "subscript", "super",
                         "switch", "throw", "throws", "true", "try", "typealias", "var",
                         "weak", "where", "while" };
    }

    return def;
}

//==============================================================================
juce::AttributedString tokenize (const juce::String& code,
                                  const juce::String& language)
{
    const auto* cfg { Whelmed::Config::getContext() };

    const juce::Font monoFont { juce::FontOptions()
                                    .withName (cfg->getString (Whelmed::Config::Key::codeFamily))
                                    .withPointHeight (cfg->getFloat (Whelmed::Config::Key::codeSize))
                                    .withStyle (cfg->getString (Whelmed::Config::Key::codeStyle)) };

    const juce::Colour fallbackColour { cfg->getColour (Whelmed::Config::Key::codeColour) };

    std::unique_ptr<juce::CodeTokeniser> tokeniser;

    if (language == "cpp" or language == "c"
        or language == "h" or language == "cc" or language == "cxx")
    {
        tokeniser = std::make_unique<juce::CPlusPlusCodeTokeniser>();
    }
    else if (language == "lua")
    {
        tokeniser = std::make_unique<juce::LuaTokeniser>();
    }
    else if (language == "xml" or language == "html" or language == "svg")
    {
        tokeniser = std::make_unique<juce::XmlTokeniser>();
    }
    else
    {
        auto def { getLanguageDefinition (language) };

        if (def.keywords.size() > 0 or def.lineComment.isNotEmpty())
            tokeniser = std::make_unique<GenericTokeniser> (def);
    }

    juce::AttributedString as;
    as.setWordWrap (juce::AttributedString::WordWrap::byWord);

    if (tokeniser == nullptr)
    {
        as.append (code, monoFont, fallbackColour);
    }
    else
    {
        const juce::Colour tokenColours[]
        {
            cfg->getColour (Whelmed::Config::Key::tokenError),
            cfg->getColour (Whelmed::Config::Key::tokenComment),
            cfg->getColour (Whelmed::Config::Key::tokenKeyword),
            cfg->getColour (Whelmed::Config::Key::tokenOperator),
            cfg->getColour (Whelmed::Config::Key::tokenIdentifier),
            cfg->getColour (Whelmed::Config::Key::tokenInteger),
            cfg->getColour (Whelmed::Config::Key::tokenFloat),
            cfg->getColour (Whelmed::Config::Key::tokenString),
            cfg->getColour (Whelmed::Config::Key::tokenBracket),
            cfg->getColour (Whelmed::Config::Key::tokenPunctuation),
            cfg->getColour (Whelmed::Config::Key::tokenPreprocessor),
        };

        static constexpr int kTokenColourCount { 11 };

        juce::CodeDocument doc;
        doc.replaceAllContent (code);

        juce::CodeDocument::Iterator snapshot { doc };
        juce::CodeDocument::Iterator it { doc };

        for (;;)
        {
            snapshot = it;
            const int tokenType { tokeniser->readNextToken (it) };
            const int tokenStart { snapshot.getPosition() };
            const int tokenEnd { it.getPosition() };

            if (tokenEnd <= tokenStart)
                break;

            const juce::String text { doc.getTextBetween (
                juce::CodeDocument::Position (doc, tokenStart),
                juce::CodeDocument::Position (doc, tokenEnd)) };

            const juce::Colour colour { (tokenType >= 0 and tokenType < kTokenColourCount)
                ? tokenColours[tokenType]
                : fallbackColour };

            as.append (text, monoFont, colour);
        }
    }

    return as;
}

/**_____________________________END OF NAMESPACE______________________________*/
} // namespace Whelmed
