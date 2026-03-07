namespace jreng
{
/*____________________________________________________________________________*/

const juce::String String::toValidID (juce::StringRef textToFormat, bool shouldBeUpperCase) noexcept
{
    // Step 1: escape & < >
    std::string t { textToFormat };
    std::string text;
    escape (t.begin(), t.end(), std::back_inserter (text));

    // Step 2: remove exclamation characters (assuming exclamation is a juce::String)
    juce::String result { juce::String (text).removeCharacters (exclamation) };

    // Step 3: prepend underscore if starts with a digit
    if (result.isNotEmpty() && result.indexOfAnyOf (numeric) == 0)
        result = underscore + result;

    // Step 4: collapse illegal characters into single underscores
    juce::String cleaned;
    cleaned.preallocateBytes (result.length() + 4);

    bool lastWasUnderscore = false;
    for (auto c : result)
    {
        // specialCharacters is const char*
        if (std::strchr (specialCharacters, (int)c) != nullptr)
        {
            if (! lastWasUnderscore)
            {
                cleaned << underscore;
                lastWasUnderscore = true;
            }
        }
        else
        {
            cleaned << c;
            lastWasUnderscore = false;
        }
    }

    return shouldBeUpperCase ? cleaned.toUpperCase() : cleaned;
}


const juce::String String::toFileExtension (juce::StringRef ext) noexcept
{
    juce::String formatted { ext };

    if (formatted.contains (asteriskDot))
        return formatted;

    juce::String tmp;
    tmp << asteriskDot << formatted;// instead of asteriskDot + formatted
    return tmp;
}

const juce::String String::toFileName (juce::StringRef name,
                                       juce::StringRef extension) noexcept
{
    juce::String tmp;
    tmp << name << dot
        << juce::String (extension).removeCharacters (asteriskDot);
    return tmp;
}

const juce::String String::getFilenameWithoutExtension (juce::StringRef filename) noexcept
{
    return juce::String (filename).upToLastOccurrenceOf (dot, false, false);
};

const juce::String String::toFullPathFileName (juce::StringRef parentDirectory,
                                               juce::StringRef filename) noexcept
{
    juce::String tmp;
    tmp << parentDirectory
        << juce::File::getSeparatorString()
        << filename;
    return tmp;
}

const juce::String String::onlyExtensionFromFilename (juce::StringRef filename) noexcept
{
    return juce::String (filename).fromLastOccurrenceOf (dot, false, false);
}

const juce::String String::toPathName (juce::StringRef name) noexcept
{
    return toValidID (juce::String (name).trim()).replace (underscore, dash).toLowerCase();
}

const juce::String String::toCapitalAbbreviation (juce::StringRef textToFormat) noexcept
{
    return forEachWord (textToFormat, [&] (juce::StringRef word)
                        {
                            if (juce::String found { getWordInArray (word, acronyms) };
                                found.isNotEmpty())
                                return found.toUpperCase();

                            return juce::String { word };
                        });
}

const juce::String String::upperFirstChar (juce::StringRef textToFormat) noexcept
{
    juce::String formatted { textToFormat };
    return formatted.replaceSection (0, 1, formatted.substring (0, 1).toUpperCase());
}

const juce::String String::lowerFirstChar (juce::StringRef textToFormat) noexcept
{
    juce::String formatted { textToFormat };
    return formatted.replaceSection (0, 1, formatted.substring (0, 1).toLowerCase());
}

const juce::String String::toTitleCase (juce::StringRef textToFormat) noexcept
{
    return forEachWord (textToFormat, [&] (juce::StringRef word)
                        {
                            return upperFirstChar (juce::String (word).toLowerCase());
                        });
}

const juce::String String::toClassName (juce::StringRef textToFormat) noexcept
{
    return toValidID (toTitleCase (textToFormat).removeCharacters (whitespace));
}

const juce::String String::toUpperCaseUnderscore (juce::StringRef textToFormat) noexcept
{
    juce::String formatted { toValidID (textToFormat) };
    return formatted.toUpperCase();
}

const juce::String String::toLowerCaseUnderscore (juce::StringRef textToFormat) noexcept
{
    juce::String formatted { toValidID (textToFormat) };
    return formatted.toLowerCase();
}

const juce::String String::toCamelCase (juce::StringRef textToFormat) noexcept
{
    return lowerFirstChar (toClassName (textToFormat));
}

const juce::String String::toUnit (juce::StringRef textToFormat) noexcept
{
    juce::String formatted { textToFormat };

    if (formatted.containsIgnoreCase ("db"))
        formatted = formatted.replace ("db", "dB", true);

    if (formatted.containsIgnoreCase ("hz"))
        formatted = formatted.replace ("hz", "Hz", true);

    return formatted;
}

const juce::String String::toNoteName (juce::StringRef textToFormat) noexcept
{
    if (not isNumber (textToFormat) and not juce::String (textToFormat).containsIgnoreCase ("k"))
    {
        juce::String formatted { upperFirstChar (textToFormat) };

        int index { -1 };
        int accidental { 0 };

        switch (formatted.length())
        {
            case 2:
                break;
            case 3:
            case 4:
            {
                if (formatted.substring (2).containsOnly (numeric))
                {
                    formatted = formatted.replaceSection (1, 1, formatted.substring (1, 2).toLowerCase());
                    juce::String octave { formatted.substring (2) };

                    if (isNoteNameFlat (formatted))
                    {
                        accidental = -1;
                        index = 12 + (juce::StringArray (flats).indexOf (formatted.substring (0, 1)) + accidental);
                        formatted = flats[index % 12] + octave;
                    }
                    else if (isNoteNameSharp (formatted))
                    {
                        accidental = 1;
                        index = 12 + (juce::StringArray (sharps).indexOf (formatted.substring (0, 1)) + accidental);
                        formatted = sharps[index % 12] + octave;
                    }
                }

                if (not accidental)
                    return juce::String {};
            }
            break;

            default:
                return juce::String {};
        }

        return formatted;
    }

    return juce::String {};
}

const juce::String String::appendWithSpace (juce::StringRef text,
                                            juce::StringRef textToAppend) noexcept
{
    juce::String result;
    result << text << whitespace << textToAppend;
    return result;
}

const juce::String String::appendWithUnderscore (juce::StringRef text,
                                                 juce::StringRef textToAppend) noexcept
{
    juce::String result;
    result << text << underscore << textToAppend;
    return result;
}

const juce::String String::prependWithUnderscore (juce::StringRef text,
                                                  juce::StringRef textToPrepend) noexcept
{
    juce::String result;
    result << textToPrepend << underscore << text;
    return result;
}


const juce::String String::removeUnderscore (juce::StringRef textWithUnderscore) noexcept
{
    return juce::String (textWithUnderscore).replace (underscore, whitespace);
}

const juce::String String::removeWhitespace (juce::StringRef textWithWhitespace) noexcept
{
    return juce::String (textWithWhitespace).removeCharacters (whitespace).trim();
}

const juce::String String::getPreColon (juce::StringRef textWithColon) noexcept
{
    return juce::String (textWithColon).upToFirstOccurrenceOf (colon, false, true).trim();
}

const juce::String String::getPostColon (juce::StringRef textWithColon) noexcept
{
    return juce::String (textWithColon).fromFirstOccurrenceOf (colon, false, true).trim();
}

const juce::String String::getYearInRoman() noexcept
{
    const char* const rom[] { "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I" };
    const int num[] { 1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1 };

    juce::String roman;

    auto year { juce::String (__DATE__).fromLastOccurrenceOf (" ", false, true).getIntValue() };

    for (auto index : indices (rom))
    {
        while (year - num[index] >= 0)
        {
            roman += rom[index];
            year -= num[index];
        }
    }

    return roman;
};

const juce::String String::toFileNameAlt (juce::StringRef filename) noexcept
{
    const juce::String name { getFilenameWithoutExtension (filename) };
    const juce::String extension { onlyExtensionFromFilename (filename) };
    const juce::String altName { appendWithUnderscore (name, IDref::alt) };

    return toFileName (altName, extension);
}

const juce::String String::replaceholder (juce::StringRef wholeString, juce::StringRef placeholder, juce::StringRef stringReplacement) noexcept
{
    juce::String toBeReplaced { doublePercent + juce::String (placeholder) + doublePercent };

    if (juce::String (wholeString).isEmpty())
        return toBeReplaced.replace (toBeReplaced, stringReplacement);

    return juce::String (wholeString).replace (toBeReplaced, stringReplacement);
}

const juce::String String::toTrademark (juce::StringRef textToBeFormatted, juce::StringRef productName) noexcept
{
#if JUCE_MODULE_AVAILABLE_juce_audio_processors
    juce::String formatted { replaceholder (textToBeFormatted, IDref::productName, productName) };
#else
    juce::String formatted { replaceholder (textToBeFormatted, IDref::productName, ProjectInfo::projectName) };
#endif

    formatted = replaceholder (formatted, IDref::companyName, companyName);
    formatted = replaceholder (formatted, IDref::legalCompanyName, legalCompanyName);

    return formatted.replace ("\\n", "\n");
}

const juce::String String::toCopyright (juce::StringRef textToBeFormatted) noexcept
{
    juce::String formatted { replaceholder (textToBeFormatted, IDref::legalCompanyName, legalCompanyName) };
    juce::String year { juce::String (copyrightSymbol) + whitespace + juce::String (__DATE__).fromLastOccurrenceOf (whitespace, false, true) };

    formatted = replaceholder (formatted, IDref::year, year);

    return formatted;
}

const juce::String String::abbreviate (juce::StringRef textToBeFormatted) noexcept
{
    juce::String formatted { removeUnderscore (textToBeFormatted) };

    juce::StringArray words;
    words.addTokens (formatted, false);

    if (words.size() > 1)
    {
        formatted.clear();
        for (auto& w : words)
            formatted += w.substring (0, 1);
    }

    return formatted.toUpperCase();
}

const juce::String String::toSVGid (juce::StringRef preColon,
                                    juce::StringRef postColon) noexcept
{
    auto post { juce::String (postColon).replace (whitespace, dash) };
    juce::String formatted { preColon + colon + dash + post };

    return formatted;
}

/**
    Check whether the passed std::string only contains valid ASCII characters 0 - 127;
 */

const bool String::isUsingStandardChars (std::string stringToCheck) noexcept
{
    for (size_t i = 0; i < stringToCheck.length(); i++)
    {
        if (stringToCheck.at (i) > 127 || stringToCheck.at (i) < 0)
            return false;
    }

    return true;
}

const juce::String String::createProductPageURL (const juce::String& productName) noexcept
{
    juce::String website { "https://kuassa.com/products/" };

    juce::StringArray words;
    words.addTokens (productName.toLowerCase(), false);

    for (auto& word : words)
        word = word.removeCharacters (specialCharacters);

    juce::String url;
    url << website << words.joinIntoString ("-");
    return url;
}


const juce::String String::createPresetExtension (const juce::String& productName) noexcept
{
    /** Create Array of 4 characters, if it contains word "JRENG!" starts with 'j', otherwise always starts with 'k' (Kuassa) and ends with 'p' (preset) */
    std::array preset {
        productName.contains ("JRENG!") ? 'j' : 'k',
        'x',
        'x',
        'p',
    };

    /** Tokenize words from product name and remove any word found in blacklist */
    juce::StringArray words;
    words.addTokens (productName.toLowerCase(), false);

    const juce::StringArray ignoreWords {
        "JRENG!",
        "Kuassa",
        "Efektor",
        "Amplifikation",
    };

    for (auto& b : ignoreWords)
        /** remove words with ignoreCase = true */
        words.removeString (b, true);

    if (not words.isEmpty())
    {
        /** replace second character in preset, with first character from first word */
        preset[1] = words[0][0];

        switch (words.size())
        {
                /** if one word */
            case 1:
            {
                if (words.size() > 2)
                {
                    /** get all consonant character from words */
                    const juce::String consonant {
                        words[0].substring (1, words[0].length() - 1).removeCharacters ("aiueo")
                    };

                    std::srand (static_cast<unsigned int> (std::time (nullptr)));
                    const int index { std::rand() % consonant.length() };

                    /** replace third character in preset with random consonant */
                    preset[2] = consonant[index];
                }
                else
                {
                    preset[2] = words[0][1];
                }
                break;
            }

            /** if two or more words, replace third character in preset with first character from second word */
            default:
                preset[2] = words[1][0];
                break;
        }
    }

    return { { preset.begin(), preset.end() } };
}

const juce::String String::toEmail (juce::StringRef mail, juce::StringRef domain) noexcept
{
    return { mail + "@" + domain };
}

const juce::String String::toDefaultPresetVersion (juce::StringRef versionString) noexcept
{
    return juce::String ("Ver." + versionString);
}

const juce::String String::fromBoolean (bool trueOrFalse) noexcept
{
    return trueOrFalse ? "true" : "false";
}

const juce::String String::dashToWhitespace (juce::StringRef text) noexcept
{
    return juce::String (text).replace (dash, whitespace);
}

//==============================================================================
const bool String::isNumber (juce::StringRef text) noexcept
{
    return juce::String (text).containsOnly (juce::String (numeric));
}

const int String::getMIDINoteNumberFromName (juce::StringRef noteName) noexcept
{
    int number { -1 };

    if (juce::String name { toNoteName (noteName) };
        name.isNotEmpty())
    {
        juce::String note { name.initialSectionNotContaining (juce::String (numeric)) };

        int octave { name.substring (note.length()).getIntValue() + 1 };

        number = (isNoteNameFlat (name) ? flats : sharps).indexOf (note) + (12 * octave);
    }

    return number;
}

const bool String::isValidNoteName (juce::StringRef noteName) noexcept
{
    return toNoteName (noteName).isNotEmpty();
}

const bool String::isNoteNameSharp (juce::StringRef noteName) noexcept
{
    if (juce::String name { noteName };
        name.length() >= 3)
        return name.substring (1, 2).compare ("#") == 0;

    return false;
}

const bool String::isNoteNameFlat (juce::StringRef noteName) noexcept
{
    if (juce::String name { noteName };
        name.length() >= 3)
        return name.substring (1, 2).compare ("b") == 0;

    return false;
}

const bool String::isKilo (juce::StringRef textNumber) noexcept
{
    const juce::String kilo { "k" };

    if (juce::String number { textNumber };
        number.endsWithIgnoreCase (kilo))
    {
        return number.upToFirstOccurrenceOf (kilo, false, true).containsAnyOf (juce::String (numeric));
    }

    return false;
}

const bool String::isVersionOld (const juce::String& versionToCheck,
                                 const juce::String& versionToCompare) noexcept
{
    if (versionToCheck.isEmpty())
        return true;
    if (versionToCompare.isEmpty())
        return false;

    juce::StringArray toCheckParts, toCompareParts;

    toCheckParts.addTokens (versionToCheck, ".", "");
    toCompareParts.addTokens (versionToCompare, ".", "");

    for (int i = 0; i < juce::jmax (toCheckParts.size(), toCompareParts.size()); ++i)
    {
        int partToCheck = i < toCheckParts.size() ? toCheckParts[i].getIntValue() : 0;
        int partToCompare = i < toCompareParts.size() ? toCompareParts[i].getIntValue() : 0;

        if (partToCheck < partToCompare)
            return true;
        if (partToCheck > partToCompare)
            return false;
    }

    return false;
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
