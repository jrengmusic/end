namespace jreng
{
/*____________________________________________________________________________*/

const std::string FuzzySearch::toLowerCase (const std::string& str)
{
    std::string lowerStr = str;
    std::transform (lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    return lowerStr;
}

const bool FuzzySearch::contains (const std::string& query, const std::string& entry)
{
    return toLowerCase (entry).find (toLowerCase (query)) != std::string::npos;
}

const FuzzySearch::Result FuzzySearch::getResult (const std::string& query, const Data::vector& dataset) noexcept
{
    Result results;

    if (query.size() < 3)
        return results;

    std::string lowerQuery = toLowerCase (query);
    for (const auto& entry : dataset)
    {
        if (entry.size() >= query.size() && contains (lowerQuery, entry))
        {
            std::size_t pos = toLowerCase (entry).find (lowerQuery);
            results.emplace_back (pos == std::string::npos ? std::numeric_limits<int>::max() : static_cast<int> (pos), entry);
        }
    }

    const auto sort = [lowerQuery] (const auto& a, const auto& b)
    {
        if (toLowerCase (a.second) == lowerQuery && toLowerCase (b.second) != lowerQuery)
            return true;

        if (toLowerCase (a.second) != lowerQuery && toLowerCase (b.second) == lowerQuery)
            return false;

        return a.first < b.first;
    };

    std::sort (results.begin(), results.end(), sort);
    return results;
}

const FuzzySearch::Result FuzzySearch::getResult (const juce::String& query, const Data::vector& dataset)
{
    return getResult (query.toStdString(), dataset);
}

const FuzzySearch::Result FuzzySearch::getResult (const std::string& query, const Data::map& dataset) noexcept
{
    Result results;

    if (query.size() < 3)
        return results;

    std::string lowerQuery = toLowerCase (query);
    for (const auto& entry : dataset)
    {
        bool key_contains = entry.first.size() >= query.size() && contains (lowerQuery, entry.first);
        bool value_contains = entry.second.size() >= query.size() && contains (lowerQuery, entry.second);

        if (key_contains || value_contains)
        {
            int key_pos = key_contains ? static_cast<int> (toLowerCase (entry.first).find (lowerQuery)) : std::numeric_limits<int>::max();
            int value_pos = value_contains ? static_cast<int> (toLowerCase (entry.second).find (lowerQuery)) : std::numeric_limits<int>::max();
            int pos = key_contains ? key_pos : value_pos + 1000;

            if (key_contains && value_contains)
            {
                pos = key_pos;
            }

            results.emplace_back (pos, entry.first + ": " + entry.second);
        }
    }

    std::sort (results.begin(), results.end(), [lowerQuery] (const auto& a, const auto& b)
               { return a.first < b.first; });

    return results;
}

//==============================================================================
const int FuzzySearch::getDistance (const std::string& s1, const std::string& s2) noexcept
{
    int length1 = static_cast<int> (s1.size());
    int length2 = static_cast<int> (s2.size());

    std::vector<std::vector<int>> d (length1 + 1, std::vector<int> (length2 + 1));
    d[0][0] = 0;

    for (int i = 1; i <= length1; ++i)
        d[i][0] = i;

    for (int i = 1; i <= length2; ++i)
        d[0][i] = i;

    for (int i = 1; i <= length1; ++i)
    {
        for (int j = 1; j <= length2; ++j)
        {
            d[i][j] = std::min (
                                { d[i - 1][j] + 1,
                                    d[i][j - 1] + 1,
                                    d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1) });
        }
    }

    return d[length1][length2];
}

//==============================================================================
const FuzzySearch::Result FuzzySearch::getResultByDistance (const std::string& query, const Data::vector& dataset) noexcept
{
    Result results;

    if (query.size() < 3)
        return results;

    std::string lowerQuery = toLowerCase (query);
    for (const auto& entry : dataset)
        if (toLowerCase (entry).find (lowerQuery) != std::string::npos)
            results.emplace_back (getDistance (lowerQuery, toLowerCase (entry)), entry);

    std::sort (results.begin(), results.end(), [] (const auto& a, const auto& b)
               { return a.first < b.first; });

    return results;
}

FuzzySearch::Result FuzzySearch::getResultByDistance (const juce::String& query, const Data::vector& dataset)
{
    return getResultByDistance (query.toStdString(), dataset);
}

const FuzzySearch::Result FuzzySearch::getResultByDistance (const std::string& query, const Data::map& dataset) noexcept
{
    Result results;

    if (query.size() < 3)
        return results;

    std::string lowerQuery = toLowerCase (query);
    for (const auto& entry : dataset)
    {
        bool key_contains = toLowerCase (entry.first).find (lowerQuery) != std::string::npos;
        bool value_contains = toLowerCase (entry.second).find (lowerQuery) != std::string::npos;

        if (key_contains || value_contains)
        {
            int key_distance = key_contains ? getDistance (lowerQuery, toLowerCase (entry.first)) : std::numeric_limits<int>::max();
            int value_distance = value_contains ? getDistance (lowerQuery, toLowerCase (entry.second)) : std::numeric_limits<int>::max();

            int offset = 1000;
            int distance = key_contains ? key_distance : value_distance + offset;

            if (key_contains && value_contains)
                distance = key_distance;

            results.emplace_back (distance, entry.first + ": " + entry.second);
        }
    }

    std::sort (results.begin(), results.end(), [] (const auto& a, const auto& b)
               { return a.first < b.first; });

    return results;
}

//==============================================================================
const bool FuzzySearch::entryExists (const std::string& entry, const Result& results) noexcept
{
    return std::any_of (results.begin(), results.end(), [&] (const auto& result)
                        { return toLowerCase (result.second) == toLowerCase (entry); });
}

const bool FuzzySearch::entryExists (const juce::String& entry, const Result& results) noexcept
{
    return entryExists (entry.toStdString(), results);
}

    /**_____________________________END OF NAMESPACE______________________________*/
} // namespace jreng
