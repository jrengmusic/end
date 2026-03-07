namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @class AnyOwner
 * @brief Type-erased heterogeneous owner for heap-allocated objects.
 *
 * AnyOwner allows you to store objects of arbitrary types in a single container,
 * while ensuring correct destruction. It uses type erasure with a custom deleter
 * and keeps track of the stored type for runtime safety checks.
 *
 * Typical usage:
 * @code
 * AnyOwner owner;
 * auto* s = owner.emplace<std::string>("hello");
 * auto* v = owner.emplace<std::vector<int>>(10, 42);
 *
 * // Safe retrieval
 * std::string* str = owner.get<std::string>(0);
 * std::vector<int>* vec = owner.get<std::vector<int>>(1);
 * @endcode
 *
 * @note Prefer Owner<Base> when a common base class exists.
 *       Use AnyOwner for edge cases where no shared base is available.
 */
class AnyOwner
{
public:
    /** @brief Default constructor. */
    AnyOwner() = default;

    /** @brief Deleted copy constructor. */
    AnyOwner (const AnyOwner&) = delete;

    /** @brief Deleted copy assignment. */
    AnyOwner& operator= (const AnyOwner&) = delete;

    /** @brief Move constructor. */
    AnyOwner (AnyOwner&&) noexcept = default;

    /** @brief Move assignment. */
    AnyOwner& operator= (AnyOwner&&) noexcept = default;

    /**
     * @brief Construct and store an object of type AnyType in place.
     *
     * @tparam AnyType The type of object to construct.
     * @tparam Args Constructor argument types.
     * @param args Arguments forwarded to AnyType's constructor.
     * @return A raw pointer to the newly created object.
     */
    template<typename AnyType, typename... Args>
    AnyType* add (Args&&... args)
    {
        auto pointer = std::make_unique<AnyType> (std::forward<Args> (args)...);
        auto raw = pointer.get();

        entries.emplace_back (
            std::unique_ptr<void, Deleter> (pointer.release(), &AnyOwner::deleter<AnyType>));
        types.emplace_back (std::type_index (typeid (AnyType)));

        return raw;
    }

    /**
     * @brief Store an already-constructed unique_ptr<T>.
     *
     * @tparam AnyType The type of the object.
     * @param ptr A unique_ptr managing the object.
     * @return A raw pointer to the stored object.
     */
    template<typename AnyType>
    AnyType* add (std::unique_ptr<AnyType> ptr)
    {
        auto raw = ptr.get();

        entries.emplace_back (
            std::unique_ptr<void, Deleter> (ptr.release(), &AnyOwner::deleter<AnyType>));
        types.emplace_back (std::type_index (typeid (AnyType)));

        return raw;
    }

    /**
     * @brief Retrieve a stored object by index with type checking (const).
     *
     * @tparam AnyType The expected type of the stored object.
     * @param index The index of the object in the container.
     * @return A const pointer to the stored object of type AnyType.
     *
     * @warning Asserts if the index is out of range or the type does not match.
     */
    template<typename AnyType>
    const AnyType* get (size_t index) const
    {
        assert (index < entries.size());
        assert (types.at (index) == std::type_index (typeid (AnyType)));
        return static_cast<const AnyType*> (entries.at (index).get());
    }

    /**
     * @brief Retrieve a stored object by index with type checking (non-const).
     *
     * @tparam AnyType The expected type of the stored object.
     * @param index The index of the object in the container.
     * @return A pointer to the stored object of type AnyType.
     *
     * @warning Asserts if the index is out of range or the type does not match.
     */
    template<typename AnyType>
    AnyType* get (size_t index)
    {
        assert (index < entries.size());
        assert (types.at (index) == std::type_index (typeid (AnyType)));
        return static_cast<AnyType*> (entries.at (index).get());
    }

    /**
     * @brief Query the number of stored objects.
     * @return The number of entries in the container.
     */
    size_t size() const noexcept { return entries.size(); }

    /**
     * @brief Remove all stored objects.
     */
    void clear() noexcept
    {
        entries.clear();
        types.clear();
    }

    /**
     * @brief Check if an index is valid.
     * @param index The index to check.
     * @return True if index is within range, false otherwise.
     */
    bool contains (size_t index) const noexcept
    {
        return index < entries.size();
    }

private:
    using Deleter = void (*) (void*);

    /**
     * @brief Type-specific deleter for stored objects.
     * @tparam T The type of object to delete.
     * @param p Pointer to the object.
     */
    template<typename T>
    static void deleter (void* p)
    {
        delete static_cast<T*> (p);
    }

    std::vector<std::unique_ptr<void, Deleter>> entries;///< Stored objects
    std::vector<std::type_index> types;///< Type info for each entry
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
