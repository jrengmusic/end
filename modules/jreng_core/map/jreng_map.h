namespace jreng::Map
{
/*____________________________________________________________________________*/

/**
 * @brief Get std::map Key from Value.
 *
 * This function inverts a std::map, creating a new map where the keys become the values and the values become the keys.
 *
 * @tparam Key The type of the keys in the original map.
 * @tparam Value The type of the values in the original map.
 * @param mapToBeInverted The original map to be inverted.
 * @return A new map with inverted keys and values.
 */
template<typename Key, typename Value>
static std::map<Value, Key> getKey (const std::map<Key, Value>& mapToBeInverted)
{
    std::map<Value, Key> inverted;
    for (const auto& pair : mapToBeInverted)
        inverted[pair.second] = pair.first;

    return inverted;
}

/**
 * @brief Get std::unordered_map Key from Value.
 *
 * This function inverts a std::unordered_map, creating a new map where the keys become the values and the values become the keys.
 *
 * @tparam Key The type of the keys in the original map.
 * @tparam Value The type of the values in the original map.
 * @param mapToBeInverted The original map to be inverted.
 * @return A new unordered_map with inverted keys and values.
 */
template<typename Key, typename Value>
static std::unordered_map<Value, Key> getKey (const std::unordered_map<Key, Value>& mapToBeInverted)
{
    std::unordered_map<Value, Key> inverted;
    for (const auto& pair : mapToBeInverted)
        inverted[pair.second] = pair.first;

    return inverted;
}

/**
 * @brief Check if a map contains a key.
 *
 * This function checks if a std::map contains a specific key.
 *
 * @tparam Key The type of the keys in the map.
 * @tparam Value The type of the values in the map.
 * @param map The map to check.
 * @param key The key to look for in the map.
 * @return True if the key is found, false otherwise.
 */
template<typename Key, typename Value>
static bool contains (const std::map<Key, Value>& map, const Key& key)
{
    if (auto it = map.find (key); it == map.end())
    {
#if JUCE_DEBUG
        jreng::debug::error ("Key not found in map: " + key.toString());
#endif
        return false;// key not found
    }
    return true;// key exists
}

/**
 * @brief Check if an unordered_map contains a key.
 *
 * This function checks if a std::unordered_map contains a specific key.
 *
 * @tparam Key The type of the keys in the map.
 * @tparam Value The type of the values in the map.
 * @param map The map to check.
 * @param key The key to look for in the map.
 * @return True if the key is found, false otherwise.
 */
template<typename Key, typename Value>
static bool contains (const std::unordered_map<Key, Value>& map, const Key& key)
{
    if (auto it = map.find (key); it == map.end())
    {
#if JUCE_DEBUG
        jreng::debug::error ("Key not found in map: " + key.toString());
#endif
        return false;// key not found
    }
    return true;// key exists
}

/**
 * @brief Check if an unordered_map contains a specific value.
 *
 * This function iterates through the elements of a std::unordered_map
 * and checks whether any of the stored values match the given value.
 *
 * @tparam Key   The type of the keys in the unordered_map.
 * @tparam Value The type of the values in the unordered_map.
 * @param map    The unordered_map to search.
 * @param value  The value to look for in the unordered_map.
 * @return True if the value is found in the map, false otherwise.
 *
 * @note This operation has linear complexity O(n), since it must
 *       check each element in the container.
 */
template<typename Key, typename Value>
static bool containsValue (const std::unordered_map<Key, Value>& map, const Value& value)
{
    return std::any_of (map.begin(), map.end(), [&value] (const auto& pair)
                        {
                            return pair.second == value;
                        });
}

#if JUCE_DEBUG
/**
 * @brief Print the keys of an unordered_map.
 *
 * This function prints the keys of a std::unordered_map using the DBGV macro for debugging purposes.
 *
 * @tparam Key The type of the keys in the map.
 * @tparam Value The type of the values in the map.
 * @param map The map to print the keys from.
 */
template<typename Key, typename Value>
static void printKeys (const std::unordered_map<Key, Value>& map)
{
    for (auto& [key, value] : map)
        std::cout << key << std::endl;
}
#endif// JUCE_DEBUG

/*____________________________________________________________________________*/
/**
 * @brief Abstract singleton-style registry base for bidirectional lookups.
 *
 * Provides a map of integer keys to juce::String values and bidirectional
 * lookup helpers. Intended to be inherited by concrete registries that
 * populate the map and define a default value via getDefault().
 *
 * Usage pattern in derived classes:
 * - Implement static instance() returning a single static instance.
 * - Provide static convenience accessors that forward to instance().
 * - Populate the protected map in the derived constructor.
 * - Implement getDefault() to return a canonical default entry.
 *
 * @note This class is abstract. The default constructor is deleted to prevent
 *       silent inheritance without acknowledging the base contract. Derived
 *       classes must call the protected templated constructor in their
 *       initializer list (e.g., : Instance(*this)).
 */
class Instance
{
public:
    /**
     * @brief Access the underlying map.
     *
     * Returns a const reference to the internal registry map that associates
     * integer keys with juce::String values. Callers cannot mutate the map.
     *
     * @return Const reference to the internal map of key-value pairs.
     */
    const auto& get() const noexcept
    {
        return map;
    }

    /**
     * @brief Reverse lookup: find the key for a given value.
     *
     * Builds an inverted view of the map (value → key) and returns the key
     * associated with the specified value.
     *
     * @param value The juce::String value to look up.
     * @return The integer key associated with the given value.
     *
     * @throws std::out_of_range If the value is not present in the map.
     */
    int get (const juce::String& value) const noexcept
    {
        return Map::getKey (map).at (value);
    }

    /**
     * @brief Forward lookup: find the value for a given key.
     *
     * Returns a const reference to the value associated with the specified key.
     * This avoids copying and makes ownership clear (value is stored in the map).
     *
     * @param key The integer key to look up.
     * @return Const reference to the juce::String associated with the key.
     *
     * @throws std::out_of_range If the key is not present in the map.
     */
    const juce::String& get (int key) const noexcept
    {
        return map.at (key);
    }

    /**
     * @brief Return the registry's default value.
     *
     * Must be implemented by derived classes to define the canonical default.
     * This allows registries whose enums/keys do not start at 1 or are sparse.
     *
     * @return Const reference to the default juce::String value.
     */
    virtual const juce::String& getDefault() const noexcept = 0;

    //==============================================================================
private:
    /**
     * @brief Deleted default constructor to prevent silent base construction.
     *
     * Derived classes must explicitly use the protected templated constructor
     * in their initializer list to acknowledge the base contract.
     */
    Instance() = delete;

    /**
     * @brief Deleted copy constructor (singleton hygiene).
     */
    Instance (const Instance&) = delete;

    /**
     * @brief Deleted move constructor (singleton hygiene).
     */
    Instance (Instance&&) noexcept = delete;

    /**
     * @brief Deleted copy assignment (singleton hygiene).
     */
    Instance& operator= (const Instance&) = delete;

    /**
     * @brief Deleted move assignment (singleton hygiene).
     */
    Instance& operator= (Instance&&) noexcept = delete;

    //==============================================================================
protected:
    /**
     * @brief Acknowledgement constructor for derived classes.
     *
     * Use this in the derived class constructor initializer list (e.g., : Instance(*this)).
     * Serves as an explicit handshake with the base to prevent silent inheritance.
     *
     * @tparam Derived The derived registry type.
     * @param derived Reference to the derived object (unused).
     */
    template<typename Derived>
    Instance (const Derived& /*derived*/)
    {
    }

    /**
     * @brief The underlying registry map.
     *
     * Holds integer keys mapped to juce::String values. Derived classes populate
     * this map in their constructor to define the registry contents.
     */
    std::map<int, juce::String> map;

    /**
     * @brief Virtual destructor for safe polymorphic deletion.
     */
    virtual ~Instance() = default;
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng::Map
