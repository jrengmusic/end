/**
 * @file LinkDetector.h
 * @brief Pure string utility for classifying terminal tokens as file paths or URLs.
 *
 * LinkDetector is a stateless header-only utility with no dependencies on the
 * terminal grid, screen state, or any JUCE component.  It provides:
 *
 *   - A built-in set of known file extensions (populated once via a static local).
 *   - Extension extraction and case-insensitive lookup.
 *   - URL protocol prefix detection.
 *   - A unified `classify()` entry point returning a `LinkType` enum.
 *
 * Config-driven extension merging (user extensions from end.lua) is intentionally
 * left to the caller — this class knows nothing about Config.
 *
 * @note All methods are `noexcept`.  No dynamic allocation occurs on the hot path
 *       after the built-in set is first constructed.
 */

#pragma once

#include <JuceHeader.h>
#include <unordered_set>
#include "../../config/Config.h"

namespace Terminal
{ /*____________________________________________________________________________*/

/**
 * @class LinkDetector
 * @brief Classifies a text token as a URL, a file path with a known extension,
 *        or neither.
 *
 * All methods are static — the class is not instantiated.  The built-in
 * extension set is constructed once on first use (static local) and reused for
 * every subsequent call.
 *
 * ### Extension extraction rules
 * | Token           | Extracted extension | Result  |
 * |-----------------|---------------------|---------|
 * | `main.cpp`      | `.cpp`              | file    |
 * | `.cpp`          | `.cpp`              | file    |
 * | `my.file.cpp`   | `.cpp`              | file    |
 * | `.gitignore`    | `.gitignore`        | file    |
 * | `noexit`        | *(none)*            | none    |
 * | `https://x.io`  | —                   | url     |
 *
 * @see LinkType
 * @see classify
 */
class LinkDetector
{
public:

    //==============================================================================
    /**
     * @enum LinkType
     * @brief Classification result returned by `classify()`.
     */
    enum class LinkType
    {
        /** Token does not match any known extension or URL protocol. */
        none,

        /** Token has a known file extension (e.g. `.cpp`, `.lua`, `.json`). */
        file,

        /** Token starts with a recognised URL protocol (`https://`, `http://`, `ftp://`). */
        url
    };

    //==============================================================================
    /**
     * @brief Returns the built-in set of known file extensions.
     *
     * The set is constructed once on first call (static local) and is never
     * modified after initialisation.  Extensions are stored in lowercase with a
     * leading dot (e.g. `".cpp"`).
     *
     * @return Const reference to the static extension set.
     */
    static const std::unordered_set<juce::String>& builtInExtensions() noexcept
    {
        static const std::unordered_set<juce::String> extensions
        {
            ".c", ".cpp", ".h", ".hpp", ".cc", ".hh",
            ".rs", ".go", ".py", ".js", ".ts", ".jsx", ".tsx",
            ".java", ".kt", ".swift", ".rb", ".lua",
            ".sh", ".bash", ".zsh", ".fish", ".pl",
            ".zig", ".asm", ".s", ".cs", ".fs",
            ".ex", ".exs", ".clj", ".scala", ".r", ".m", ".mm",
            ".md", ".txt", ".json", ".yaml", ".yml", ".toml",
            ".xml", ".html", ".css", ".csv",
            ".sql", ".graphql", ".proto",
            ".makefile", ".cmake", ".dockerfile",
            ".gitignore", ".editorconfig", ".env", ".ini", ".cfg", ".conf"
        };

        return extensions;
    }

    //==============================================================================
    /**
     * @brief Returns true when @p token ends with a known file extension.
     *
     * Extension extraction uses `juce::String::fromLastOccurrenceOf(".", true, false)`
     * so only the suffix from the final dot onward is tested.  A token that
     * begins with a dot and has no subsequent dot is treated as a hidden-file
     * extension (e.g. `.gitignore` → extension `.gitignore`).
     *
     * Comparison is case-insensitive so `.CPP`, `.Cpp`, and `.cpp` all match.
     *
     * @param token  The text token to test (e.g. `"main.cpp"`, `".gitignore"`).
     * @return `true` if the extracted extension is in the built-in set.
     */
    static bool hasKnownExtension (const juce::String& token) noexcept
    {
        const auto dotIndex { token.lastIndexOfChar ('.') };
        const bool hasDot { dotIndex >= 0 };
        bool result { false };

        if (hasDot)
        {
            const juce::String ext { token.fromLastOccurrenceOf (".", true, false).toLowerCase() };
            result = builtInExtensions().count (ext) > 0
                     or Config::getContext()->isClickableExtension (ext);
        }

        return result;
    }

    //==============================================================================
    /**
     * @brief Returns true when @p token begins with a recognised URL protocol.
     *
     * Recognised protocols: `https://`, `http://`, `ftp://`.
     * Comparison is case-insensitive.
     *
     * @param token  The text token to test.
     * @return `true` if @p token starts with any recognised protocol prefix.
     */
    static bool isUrl (const juce::String& token) noexcept
    {
        return token.startsWithIgnoreCase ("https://")
            or token.startsWithIgnoreCase ("http://")
            or token.startsWithIgnoreCase ("ftp://");
    }

    //==============================================================================
    /**
     * @brief Classifies @p token as a URL, a file path, or neither.
     *
     * Evaluation order:
     * 1. `isUrl()` — returns `LinkType::url` if matched.
     * 2. `hasKnownExtension()` — returns `LinkType::file` if matched.
     * 3. Returns `LinkType::none`.
     *
     * @param token  The text token to classify.
     * @return A `LinkType` value indicating the classification result.
     * @see isUrl
     * @see hasKnownExtension
     */
    static LinkType classify (const juce::String& token) noexcept
    {
        LinkType result { LinkType::none };

        if (isUrl (token))
            result = LinkType::url;
        else if (hasKnownExtension (token))
            result = LinkType::file;

        return result;
    }
};

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal
