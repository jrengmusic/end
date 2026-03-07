#pragma once

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @brief A wrapper around std::vector specifically designed for holding objects.
 *
 * This class gives std::vector interface similar to juce::OwnedArray, but you
 * still can use as std::vector, i.e with std::algorithm.
 *
 * std::for_each (myOwner.begin(), myOwner.end(), [] (auto& item) { // do something for each item });
 *
 * This holds a list of pointers to objects, and will automatically
 * delete the objects when they are removed from the array, or when the
 * array is itself deleted.
 *
 * Declare it in the form:
 *
 *     Owner<ObjectClass> myOwner;
 *
 * ..and then add new objects, either one of the following will work just fine :
 *
 *     myOwner.add (std::make_unique<MyObjectClass>());
 *
 *     auto item = std::make_unique<MyObjectClass> {};
 *     myOwner.add (std::move (item));
 *
 * After adding objects, they are owned by the Owner and will be deleted when
 * removed or replaced.
 */

template<typename ObjectClass>
class Owner : public std::vector<std::unique_ptr<ObjectClass>>
{
public:
    using const_iterator = typename std::vector<std::unique_ptr<ObjectClass>>::const_iterator;
    using iterator = typename std::vector<std::unique_ptr<ObjectClass>>::iterator;

    //==============================================================================
    Owner() = default;
    ~Owner() = default;

    //==============================================================================
    // Constructor using `std::initializer_list` with `std::move`
    /**
     * @brief Constructor using `std::initializer_list` with `std::move`.
     *
     * @param list The initializer list to use for constructing the Owner.
     */
    Owner (std::initializer_list<std::unique_ptr<ObjectClass>> list)
    {
        // Create a temporary vector to hold the elements
        std::vector<std::unique_ptr<ObjectClass>> tempList;

        for (auto& item : list)
            tempList.push_back (std::move (const_cast<std::unique_ptr<ObjectClass>&> (item)));

        // Move the elements into the owner
        for (auto& item : tempList)
            std::vector<std::unique_ptr<ObjectClass>>::emplace_back (std::move (item));
    }

    /**
     * @brief Constructor to create an Owner with a specified size and arguments.
     *
     * @tparam Args The types of the arguments to pass to the object's constructor.
     * @param size The number of objects to create.
     * @param args The arguments to pass to the object's constructor.
     */
    template<typename... Args>
    Owner (size_t size, Args&&... args)
    {
        for (size_t i = 0; i < size; ++i)
            this->emplace_back (std::make_unique<ObjectClass> (std::forward<Args> (args)...));
    }

    //==============================================================================
    /**
     * @brief Adds a new object to the end of the Vector.
     *
     * @param newObject The `unique_ptr` object to add to the array.
     * @returns `unique_ptr` to the added object.
     */
    std::unique_ptr<ObjectClass>& add (std::unique_ptr<ObjectClass>&& newObject)
    {
        this->emplace_back (std::move (newObject));
        return this->back();
    }

    //==============================================================================
    /**
     * @brief Adds an object unless it's already in the array.
     *
     * @param newObject The `unique_ptr` object to add to the array.
     * @returns true if the object was added; false if it was already there.
     */
    bool addIfNotAlreadyThere (std::unique_ptr<ObjectClass>&& newObject) noexcept
    {
        if (contains (newObject))
            return false;

        add (std::move (newObject));
        return true;
    }

    //==============================================================================
    /**
     * @brief Checks if the Vector contains a specific object.
     *
     * @param objectToLookFor The object to look for in the array.
     * @returns true if the object is found, false otherwise.
     */
    bool contains (const std::unique_ptr<ObjectClass>& objectToLookFor) const noexcept
    {
        return getConstIterator (objectToLookFor) != this->cend();
    }

    //==============================================================================
    /**
     * @brief Finds the index of an object which might be in the Vector.
     *
     * @param objectToLookFor The object to look for in the array.
     * @returns The index of the object if found, -1 otherwise.
     */
    int indexOf (const std::unique_ptr<ObjectClass>& objectToLookFor) const noexcept
    {
        auto it = getConstIterator (objectToLookFor);
        if (it != this->cend())
        {
            return static_cast<int> (std::distance (this->begin(), it));
        }
        return -1;
    }

    /**
     * @brief Finds the index of an object by raw pointer.
     */
    int indexOf (const ObjectClass* objectToLookFor) const noexcept
    {
        auto it = std::find_if (this->begin(), this->end(), [objectToLookFor] (auto const& ptr)
                                {
                                    return ptr.get() == objectToLookFor;
                                });
        if (it != this->end())
            return static_cast<int> (std::distance (this->begin(), it));
        return -1;
    }

    //==============================================================================
    /**
     * @brief Removes an object from the Vector.
     *
     * @param indexToRemove The index of the object to remove.
     */
    void remove (int indexToRemove)
    {
        this->erase (this->begin() + indexToRemove);
    }

    void remove (size_t indexToRemove)
    {
        this->erase (this->begin() + indexToRemove);
    }

    //==============================================================================
    /**
     * @brief Removes a range of objects from the Vector.
     *
     * @param startIndex The starting index of the range to remove.
     * @param numberToRemove The number of objects to remove.
     */
    void removeRange (int startIndex, int numberToRemove)
    {
        auto start { this->begin() + startIndex };
        auto number { start + numberToRemove };
        this->erase (start, number);
    }

    //==============================================================================
    /** Get a constant iterator to an object.
     *
     * @param objectToLookFor The object to look for in the array.
     * @returns A constant iterator to the object.
     */
    const_iterator cit (const std::unique_ptr<ObjectClass>& objectToLookFor) const noexcept
    {
        return getConstIterator (objectToLookFor);
    }

    /** Get a constant iterator to an index.
     *
     * @param index The index to get the iterator for.
     * @returns A constant iterator to the index.
     */
    const_iterator cit (int index) const noexcept
    {
        return this->begin() + index;
    }

    /** Get an iterator to an object.
     *
     * @param objectToLookFor The object to look for in the array.
     * @returns An iterator to the object.
     */
    iterator it (const std::unique_ptr<ObjectClass>& objectToLookFor) noexcept
    {
        return getIterator (objectToLookFor);
    }

    /** Get an iterator to an index.
     *
     * @param index The index to get the iterator for.
     * @returns An iterator to the index.
     */
    iterator it (int index) noexcept
    {
        return this->begin() + index;
    }

    //==============================================================================
    /**
     * @brief Check if an object is at a specific index.
     *
     * @tparam PointerRefOrInt The type of the pointer or index.
     * @param objectToLookFor The object to look for in the array.
     * @param index The index to check.
     * @returns true if the object is at the index, false otherwise.
     */
    template<typename PointerRefOrInt>
    bool is (const PointerRefOrInt& objectToLookFor, int index) noexcept
    {
        return cit (objectToLookFor) == cit (index);
    }

    //==============================================================================
    /**
     * @brief Check if the array is empty.
     *
     * @returns true if the array is empty, false otherwise.
     */
    inline bool isEmpty() const noexcept
    {
        return this->size() == 0;
    }

private:
    //==============================================================================
    /** Get a constant iterator to an object.
     *
     * @param objectToLookFor The object to look for in the array.
     * @returns A constant iterator to the object.
     */
    const_iterator getConstIterator (const std::unique_ptr<ObjectClass>& objectToLookFor) const noexcept
    {
        return std::find (this->cbegin(), this->cend(), objectToLookFor);
    }

    /** Get an iterator to an object.
     *
     * @param objectToLookFor The object to look for in the array.
     * @returns An iterator to the object.
     */
    iterator getIterator (const std::unique_ptr<ObjectClass>& objectToLookFor) noexcept
    {
        return std::find (this->begin(), this->end(), objectToLookFor);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Owner)
};

/*____________________________________________________________________________*/

/**
 * @brief Helper to make an array at compile time.
 *
 * This function creates an array initialized with the given value, using a provided index sequence.
 *
 * @tparam IndexSequence The index sequence.
 * @tparam ValueType The type of the value to initialize the array with.
 * @param initValue The value to initialize the array with.
 * @param std::index_sequence<IndexSequence...> The index sequence.
 * @return An array initialized with the given value.
 */
template<std::size_t... IndexSequence, typename ValueType>
constexpr std::array<ValueType, sizeof...(IndexSequence)>
make_array (const ValueType& initValue, std::index_sequence<IndexSequence...>)
{
    return { { (void (IndexSequence), initValue)... } };
}

/**
 * @brief Helper to make an array at compile time.
 *
 * This function creates an array of the specified size, initialized with the given value.
 *
 * @tparam Size The size of the array.
 * @tparam ValueType The type of the value to initialize the array with.
 * @param initValue The value to initialize the array with.
 * @return An array initialized with the given value.
 */
template<std::size_t Size, typename ValueType>
constexpr std::array<ValueType, Size> make_array (const ValueType& initValue)
{
    return make_array (initValue, std::make_index_sequence<Size>());
}

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
