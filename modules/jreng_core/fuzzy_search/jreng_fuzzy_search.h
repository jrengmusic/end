namespace jreng
{
/*____________________________________________________________________________*/
/**
 * @brief A struct providing various fuzzy search functionalities.
 */
struct FuzzySearch
{
    /**
     * @brief Contains type definitions for vector and map datasets.
     */
    struct Data
    {
        using vector = std::vector<std::string>; /**< Type definition for vector dataset. */
        using map = std::unordered_map<std::string, std::string>; /**< Type definition for map dataset. */
    };

    /**
     * @brief Type definition for search results containing position/distance and entry pairs.
     */
    using Result = std::vector<std::pair<int, std::string>>;

    /**
     * @brief Converts a string to lower case.
     * @param str The input string.
     * @return The lower-cased version of the input string.
     */
    static const std::string toLowerCase (const std::string& str);

    /**
     * @brief Checks if a string contains a query, case-insensitive.
     * @param query The search query.
     * @param entry The string to check.
     * @return True if the entry contains the query, false otherwise.
     */
    static const bool contains (const std::string& query, const std::string& entry);

    /**
     * @brief Performs a fuzzy search on a vector dataset.
     * @param query The search query.
     * @param dataset The vector dataset to search.
     * @return A collection of matching results.
     */
    static const Result getResult (const std::string& query, const Data::vector& dataset) noexcept;

    /**
     * @brief Performs a fuzzy search on a vector dataset using JUCE's String.
     * @param query The search query in JUCE's String format.
     * @param dataset The vector dataset to search.
     * @return A collection of matching results.
     */
    static const Result getResult (const juce::String& query, const Data::vector& dataset);

    /**
     * @brief Performs a fuzzy search on a map dataset.
     * @param query The search query.
     * @param dataset The map dataset to search.
     * @return A collection of matching results.
     */
    static const Result getResult (const std::string& query, const Data::map& dataset) noexcept;

    /**
     * @brief Calculates the Levenshtein distance between two strings.
     * @param s1 The first string.
     * @param s2 The second string.
     * @return The Levenshtein distance.
     */
    static const int getDistance (const std::string& s1, const std::string& s2) noexcept;

    /**
     * @brief Performs a fuzzy search by Levenshtein distance on a vector dataset.
     * @param query The search query.
     * @param dataset The vector dataset to search.
     * @return A collection of matching results sorted by distance.
     */
    static const Result getResultByDistance (const std::string& query, const Data::vector& dataset) noexcept;

    /**
     * @brief Performs a fuzzy search by Levenshtein distance on a vector dataset using JUCE's String.
     * @param query The search query in JUCE's String format.
     * @param dataset The vector dataset to search.
     * @return A collection of matching results sorted by distance.
     */
    static Result getResultByDistance (const juce::String& query, const Data::vector& dataset);

    /**
     * @brief Performs a fuzzy search by Levenshtein distance on a map dataset.
     * @param query The search query.
     * @param dataset The map dataset to search.
     * @return A collection of matching results sorted by distance.
     */
    static const Result getResultByDistance (const std::string& query, const Data::map& dataset) noexcept;

    /**
     * @brief Checks if a specific entry exists in the search results.
     * @param entry The entry to check.
     * @param results The search results to verify against.
     * @return True if the entry exists, false otherwise.
     */
    static const bool entryExists (const std::string& entry, const Result& results) noexcept;

    /**
     * @brief Checks if a specific entry exists in the search results using JUCE's String.
     * @param entry The entry to check in JUCE's String format.
     * @param results The search results to verify against.
     * @return True if the entry exists, false otherwise.
     */
    static const bool entryExists (const juce::String& entry, const Result& results) noexcept;
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
