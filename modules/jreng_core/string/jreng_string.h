
namespace jreng
{
/*____________________________________________________________________________*/

class String final
{
public:
    //==============================================================================
    inline static const char* const numeric { "01234567890.-" };
    inline static const char* const specialCharacters { ". `~!@#$%^&*()-=+[]{}\\|;:'\",.<>/?" };
    inline static const char* const underscore { "_" };
    inline static const char* const dash { "-" };
    inline static const char* const whitespace { " " };
    inline static const char* const dot { "." };
    inline static const char* const asterisk { "*" };
    inline static const char* const asteriskDot { "*." };
    inline static const char* const colon { ":" };
    inline static const char* const exclamation { "!" };
    inline static const char* const hertz { "Hz" };
    inline static const char* const kiloHertz { "KHz" };
    inline static const char* const doublePercent { "%%" };
    inline static const juce::CharPointer_UTF8 copyrightSymbol { "\xc2\xa9" };

    //==============================================================================
private:
    inline static const juce::StringArray acronyms { "hpf", "lpf", "hf", "hmf", "mf", "lmf", "lf", "eq" };
    inline static const juce::StringArray sharps { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline static const juce::StringArray flats { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    //==============================================================================

    static const juce::String getWordInArray (juce::StringRef textMightContainsWord, const juce::StringArray& wordsArray) noexcept
    {
        for (auto& w : wordsArray)
        {
            juce::String word { textMightContainsWord };

            if (word.containsWholeWord (w))
                return w;
        }

        return {};
    }

    template<typename Function>
    static const juce::String forEachWord (juce::StringRef text, const Function& function)
    {
        auto words { juce::StringArray::fromTokens (text, false) };

        for (auto& w : words)
        {
            words.set (words.indexOf (w), function (w));
        }

        return words.joinIntoString (" ");
    }

    /** Helper for null-terminated ASCII strings (no end of string iterator). */
    template<typename InIter, typename OutIter>
    static const OutIter copy_asciiz (InIter begin, OutIter out)
    {
        while (*begin != '\0')
            *out++ = *begin++;

        return (out);
    }

    /** XML escaping in it's general form.  Note that 'out' is expected
    to an "infinite" sequence. */

    template<typename InIter, typename OutIter>
    static const OutIter escape (InIter begin, InIter end, OutIter out)
    {
        static const char bad[] = "&<>";
        static const char* rep[] = { "&amp;", "&lt;", "&gt;" };
        static const std::size_t n = sizeof (bad) / sizeof (bad[0]);

        for (; (begin != end); ++begin)
        {
            // Find which replacement to use.
            const std::size_t i = std::distance (bad, std::find (bad, bad + n, *begin));

            if (i == n)
                *out++ = *begin;// No need for escaping.
            else
                out = copy_asciiz (rep[i], out);// Escape the character.
        }

        return (out);
    }

    //==============================================================================

public:
    /**
     * @brief Converts a string into a valid identifier.
     *
     * Removes invalid characters, replaces special characters with underscores,
     * and optionally uppercases the result.
     *
     * @param textToFormat Input text to convert.
     * @param shouldBeUpperCase If true, result is uppercased.
     * @return A valid identifier string.
     */
    static const juce::String toValidID (juce::StringRef textToFormat,
                                         bool shouldBeUpperCase = false) noexcept;

    /**
     * @brief Ensures a file extension string is prefixed with "*.".
     *
     * @param ext File extension text.
     * @return Extension string in "*.ext" format.
     */
    static const juce::String toFileExtension (juce::StringRef ext) noexcept;

    /**
     * @brief Builds a filename with extension.
     *
     * @param name Base filename.
     * @param extension Extension to append.
     * @return Combined filename with extension.
     */
    static const juce::String toFileName (juce::StringRef name,
                                          juce::StringRef extension) noexcept;

    /**
     * @brief Extracts the filename without its extension.
     *
     * @param filename Full filename.
     * @return Filename without extension.
     */
    static const juce::String getFilenameWithoutExtension (juce::StringRef filename) noexcept;

    /**
     * @brief Builds a full path string from directory and filename.
     *
     * @param parentDirectory Directory path.
     * @param filename File name.
     * @return Full path string.
     */
    static const juce::String toFullPathFileName (juce::StringRef parentDirectory,
                                                  juce::StringRef filename) noexcept;

    /**
     * @brief Extracts only the extension from a filename.
     *
     * @param filename Full filename.
     * @return Extension string (including dot).
     */
    static const juce::String onlyExtensionFromFilename (juce::StringRef filename) noexcept;

    /**
     * @brief Converts a name into a path‑safe string.
     *
     * @param name Input name.
     * @return Path‑safe string.
     */
    static const juce::String toPathName (juce::StringRef name) noexcept;

    /**
     * @brief Converts text into an uppercase abbreviation.
     *
     * @param textToFormat Input text.
     * @return Abbreviation in uppercase.
     */
    static const juce::String toCapitalAbbreviation (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Uppercases the first character of a string.
     *
     * @param textToFormat Input text.
     * @return String with first character uppercased.
     */
    static const juce::String upperFirstChar (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Lowercases the first character of a string.
     *
     * @param textToFormat Input text.
     * @return String with first character lowercased.
     */
    static const juce::String lowerFirstChar (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Converts text to title case (each word capitalized).
     *
     * @param textToFormat Input text.
     * @return Title‑cased string.
     */
    static const juce::String toTitleCase (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Converts text into a valid class name.
     *
     * Removes whitespace and applies title case.
     *
     * @param textToFormat Input text.
     * @return Valid class name string.
     */
    static const juce::String toClassName (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Converts text into uppercase with underscores.
     *
     * @param textToFormat Input text.
     * @return Uppercase underscore string.
     */
    static const juce::String toUpperCaseUnderscore (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Converts text into lowercase with underscores.
     *
     * @param textToFormat Input text.
     * @return Lowercase underscore string.
     */
    static const juce::String toLowerCaseUnderscore (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Converts text into camelCase.
     *
     * @param textToFormat Input text.
     * @return camelCase string.
     */
    static const juce::String toCamelCase (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Formats a unit string (e.g. "db" → "dB", "hz" → "Hz").
     *
     * @param textToFormat Input text.
     * @return Formatted unit string.
     */
    static const juce::String toUnit (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Converts text into a normalized musical note name.
     *
     * @param textToFormat Input text.
     * @return Note name string or empty if invalid.
     */
    static const juce::String toNoteName (juce::StringRef textToFormat) noexcept;

    /**
     * @brief Appends text with a space separator.
     */
    static const juce::String appendWithSpace (juce::StringRef text,
                                               juce::StringRef textToAppend) noexcept;

    /**
     * @brief Appends text with an underscore separator.
     */
    static const juce::String appendWithUnderscore (juce::StringRef text,
                                                    juce::StringRef textToAppend) noexcept;

    /**
     * @brief Prepends text with an underscore separator.
     */
    static const juce::String prependWithUnderscore (juce::StringRef text,
                                                     juce::StringRef textToPrepend) noexcept;

    /**
     * @brief Removes underscores from a string.
     */
    static const juce::String removeUnderscore (juce::StringRef textWithUnderscore) noexcept;

    /**
     * @brief Removes whitespace from a string.
     */
    static const juce::String removeWhitespace (juce::StringRef textWithWhitespace) noexcept;

    /**
     * @brief Extracts substring before a colon.
     */
    static const juce::String getPreColon (juce::StringRef textWithColon) noexcept;

    /**
     * @brief Extracts substring after a colon.
     */
    static const juce::String getPostColon (juce::StringRef textWithColon) noexcept;

    /**
     * @brief Returns the current year in Roman numerals.
     */
    static const juce::String getYearInRoman() noexcept;

    /**
     * @brief Converts a filename into an alternate safe form.
     */
    static const juce::String toFileNameAlt (juce::StringRef filename) noexcept;

    /**
     * @brief Replaces a placeholder substring with another string.
     */
    static const juce::String replaceholder (juce::StringRef wholeString,
                                             juce::StringRef placeholder,
                                             juce::StringRef stringReplacement) noexcept;

    /**
     * @brief Formats text with a trademark symbol for a product.
     */
    static const juce::String toTrademark (juce::StringRef textToBeFormatted,
                                           juce::StringRef productName) noexcept;

    /**
     * @brief Formats text with a copyright symbol.
     */
    static const juce::String toCopyright (juce::StringRef textToBeFormatted) noexcept;

    /**
     * @brief Abbreviates a string.
     */
    static const juce::String abbreviate (juce::StringRef textToBeFormatted) noexcept;

    /**
     * @brief Builds an SVG id string from pre‑ and post‑colon parts.
     */
    static const juce::String toSVGid (juce::StringRef preColon,
                                       juce::StringRef postColon) noexcept;

    /**
     * @brief Checks if a string uses only standard characters.
     */
    static const bool isUsingStandardChars (std::string stringToCheck) noexcept;

    /**
     * @brief Creates a product page URL from a product name.
     */
    static const juce::String createProductPageURL (const juce::String& productName) noexcept;

    /**
     * @brief Creates a preset file extension from a product name.
     */
    static const juce::String createPresetExtension (const juce::String& productName) noexcept;

    /**
     * @brief Builds an email address from local part and domain.
     */
    static const juce::String toEmail (juce::StringRef mail,
                                       juce::StringRef domain = IDref::kuassa + ".com") noexcept;

    /**
     * @brief Formats a version string as a default preset version.
     */
    static const juce::String toDefaultPresetVersion (juce::StringRef versionString) noexcept;

    /**
     * @brief Converts a boolean to "true" or "false".
     */
    static const juce::String fromBoolean (bool trueOrFalse) noexcept;
    
    /**
     * @brief Removes "-" from string, and replace it with whitespace;
     */
    static const juce::String dashToWhitespace (juce::StringRef text) noexcept;

    //==============================================================================

    /**
     * @brief Checks if a string is numeric.
     */
    static const bool isNumber (juce::StringRef text) noexcept;

    /**
     * @brief Converts a note name to a MIDI note number.
     */
    static const int getMIDINoteNumberFromName (juce::StringRef noteName) noexcept;

    /**
     * @brief Checks if a note name is valid.
     */
    static const bool isValidNoteName (juce::StringRef noteName) noexcept;

    /**
     * @brief Checks if a note name is sharp.
     */
    static const bool isNoteNameSharp (juce::StringRef noteName) noexcept;

    /**
     * @brief Checks if a note name is flat.
     */
    static const bool isNoteNameFlat (juce::StringRef noteName) noexcept;

    /**
     * @brief Checks if a string represents a kilo value (e.g. "k").
     */
    static const bool isKilo (juce::StringRef textNumber) noexcept;

    /**
     * @brief Compares two version strings to see if one is older.
     */
    static const bool isVersionOld (const juce::String& versionToCheck,
                                    const juce::String& versionToCompare) noexcept;

    /**
     * @brief Converts a frequency in Hz to a MIDI note number.
     *
     * Uses the standard formula:
     *   note = 12 * log2(frequency / referencePitch) + 69
     *
     * @tparam ValueType Floating‑point type (e.g. float, double).
     * @param frequencyInHz Frequency in Hz to convert.
     * @param standardPitchReference Reference pitch for A4 (default = 440 Hz).
     * @return MIDI note number corresponding to the frequency.
     */
    template<typename ValueType>
    static const int getMIDINoteNumberFromHz (ValueType& frequencyInHz,
                                              ValueType standardPitchReference = 440)
    {
        return toInt (12 * std::log2 (frequencyInHz / standardPitchReference) + 69);
    }

    /**
     * @brief Converts a MIDI note number to its frequency in Hz.
     *
     * Uses the formula:
     *   frequency = referencePitch * 2^((noteNumber - 69) / 12)
     *
     * @tparam ValueType Floating‑point type (e.g. float, double).
     * @param noteNumber MIDI note number (0–163 valid).
     * @param standardPitchReference Reference pitch for A4 (default = 440 Hz).
     * @return Frequency in Hz, or the noteNumber itself if out of range.
     */
    template<typename ValueType>
    static const ValueType getFrequencyInHzFromNoteNumber (int noteNumber,
                                                           ValueType standardPitchReference = 440)
    {
        if (juce::isPositiveAndBelow (noteNumber, 164))
            return static_cast<ValueType> (standardPitchReference * std::pow (2, (static_cast<ValueType> (noteNumber) - static_cast<ValueType> (69)) / static_cast<ValueType> (12)));

        return noteNumber;
    }

    /**
     * @brief Converts a note name (e.g. "C#4") to its frequency in Hz.
     *
     * Internally resolves the note name to a MIDI note number, then converts
     * that number to frequency using @ref getFrequencyInHzFromNoteNumber.
     *
     * @tparam ValueType Floating‑point type (e.g. float, double).
     * @param noteName Musical note name string.
     * @return Frequency in Hz corresponding to the note name.
     */
    template<typename ValueType>
    static const ValueType getFrequencyInHzFromNoteName (const juce::String& noteName)
    {
        return getFrequencyInHzFromNoteNumber<ValueType> (getMIDINoteNumberFromName (noteName));
    }

    /**
     * @brief Converts a frequency in Hz to a musical note name.
     *
     * Optionally formats with sharps or flats, and includes octave number.
     *
     * @tparam ValueType Floating‑point type (e.g. float, double).
     * @param frequencyInHz Frequency in Hz to convert.
     * @param useSharps If true, use sharps (#); otherwise use flats (b).
     * @param includeOctaveNumber If true, append octave number to note name.
     * @param octaveNumForMiddleC Octave number to assign to middle C (default = 4).
     * @return Note name string (e.g. "A4", "C#3"), or empty if out of range.
     */
    template<typename ValueType>
    static juce::String getMIDINoteNameFromHz (ValueType frequencyInHz,
                                               bool useSharps = true,
                                               bool includeOctaveNumber = true,
                                               int octaveNumForMiddleC = 4)
    {
        int note { getMIDINoteNumberFromHz (frequencyInHz) };

        if (juce::isPositiveAndBelow (note, 164))
        {
            juce::String s (useSharps ? sharps[note % 12]
                                      : flats[note % 12]);

            if (includeOctaveNumber)
                s << (note / 12 + (octaveNumForMiddleC - 5));

            return s;
        }

        return {};
    }

};

//==============================================================================
//template <String::FormatType type, typename... Args>
//auto format (Args&&... args) -> decltype (String::format<type> (std::forward<Args> (args)...))
//{
//    return String::format<type> (std::forward<Args> (args)...);
//}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
