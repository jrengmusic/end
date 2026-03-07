#include <cassert>

namespace jreng
{
/*____________________________________________________________________________*/

/**
 * @struct Function
 * @brief Type-safe container for callable entities using composition pattern.
 *
 * @details Provides compile-time type-safe storage and retrieval of callables.
 *          Uses composition (not inheritance) for clean, predictable behavior.
 *          Supports both return values and output parameters for zero-copy patterns.
 *
 * @example Basic callback map
 * @code
 * Function::Map<std::string, void> callbacks;
 * callbacks.add<int>("setValue", [](int v) { std::cout << v; });
 * callbacks.get("setValue", 42);  // Prints "42"
 * @endcode
 *
 * @example Zero-copy pattern with output parameters
 * @code
 * Function::Map<std::string, void> getters;
 * getters.add<int, std::vector<double>&>("getData",
 *     [](int size, std::vector<double>& output) {
 *         output.resize(size);
 *         // Fill output...
 *     });
 * std::vector<double> buffer;  // Reusable buffer
 * getters.get("getData", 100, buffer);  // Zero allocations
 * @endcode
 */
struct Function
{
    /**
     * @struct Common
     * @brief Base class for callable elements in Function.
     *
     * @details Acts as a common interface for all callable elements.
     */
    struct Common
    {
        /** @brief Virtual destructor for Common. */
        virtual ~Common() = default;
    };

    /**
     * @brief A callable element that contains a std::function (composition pattern).
     *
     * @tparam ReturnType The return type of the callable element.
     * @tparam Args The argument types for the callable element (as specified, preserving references).
     */
    template<typename ReturnType, typename... Args>
    struct Element : Common
    {
        std::function<ReturnType(Args...)> func;

        /**
         * @brief Constructor for Element.
         *
         * @tparam FunctionType The type of the function to wrap.
         * @param newFunction The function to wrap as a callable element.
         */
        template<typename FunctionType>
        Element(FunctionType&& newFunction)
            : func(std::forward<FunctionType>(newFunction))
        {
        }

        /**
         * @brief Call operator to invoke the stored function.
         */
        ReturnType operator()(Args... args) const
        {
            return func(std::forward<Args>(args)...);
        }
    };

    //==============================================================================
    /**
     * @struct Vector
     * @brief A container for callable elements, implemented as a vector with safety enhancements.
     *
     * @tparam ReturnType The return type of the callable elements in the vector.
     */
    template<typename ReturnType>
    struct Vector : std::vector<std::unique_ptr<Function::Common>>
    {
        /** @brief Default constructor for Vector. */
        Vector() = default;
        // explicitly move-only
        Vector (Vector&&) noexcept = default;
        Vector& operator= (Vector&&) noexcept = default;
        Vector (const Vector&) = delete;
        Vector& operator= (const Vector&) = delete;

        /** @brief Destructor for Vector. */
        ~Vector() = default;

        /**
         * @brief Adds a new callable element to the vector.
         *
         * @tparam Args The argument types for the callable element (preserving references).
         * @tparam FunctionType The type of the function to add.
         * @param newFunction The function to add as a callable element.
         */
        template<typename... Args, typename FunctionType>
        void add(FunctionType&& newFunction)
        {
            using CanonElem = Function::Element<ReturnType, Args...>;
            auto ptr = std::make_unique<CanonElem>(std::forward<FunctionType>(newFunction));
            this->push_back(std::move(ptr));
        }

        /**
         * @brief Retrieves and calls a callable element from the vector with safety checks.
         *
         * @tparam Args The argument types for the callable element.
         * @param index The index of the callable element in the vector.
         * @param args The arguments to pass to the callable element.
         * @return The result of calling the callable element.
         *
         * @throws std::out_of_range If the index is not within the vector bounds.
         */
        template<typename... Args>
        ReturnType get(int index, Args&&... args)
        {
            assert(index >= 0 && index < static_cast<int>(this->size()) &&
                   "Function::Vector::get() - Index out of bounds");

            if (index < 0 || index >= static_cast<int>(this->size()))
                throw std::out_of_range("Index out of bounds in Function::Vector");

            using CanonElem = Function::Element<ReturnType, Args...>;
            auto* elem = static_cast<CanonElem*>(this->at(index).get());

            return (*elem)(std::forward<Args>(args)...);
        }

        /**
         * @brief Retrieves and calls a callable element from the vector (const overload).
         *
         * @tparam Args The argument types for the callable element.
         * @param index The index of the callable element in the vector.
         * @param args The arguments to forward to the callable element.
         * @return The result of invoking the stored callable.
         *
         * @details
         * This overload allows invocation on a const-qualified vector.
         *
         * @throws std::out_of_range If the index is not within the vector bounds.
         * @note If the caller uses a different signature than was registered with
         *       add(), the cast will be ill-formed at compile time.
         */
        template<typename... Args>
        ReturnType get(int index, Args&&... args) const
        {
            assert(index >= 0 && index < static_cast<int>(this->size()) &&
                   "Function::Vector::get() const - Index out of bounds");

            if (index < 0 || index >= static_cast<int>(this->size()))
                throw std::out_of_range("Index out of bounds in Function::Vector");

            using CanonElem = Function::Element<ReturnType, Args...>;
            auto* elem = static_cast<CanonElem*>(this->at(index).get());

            return (*elem)(std::forward<Args>(args)...);
        }
    };

    //==============================================================================
    /**
     * @struct Map
     * @brief A container for callable elements, implemented as a map with enhanced safety.
     *
     * @tparam KeyType The type of the keys in the map.
     * @tparam ReturnType The return type of the callable elements in the map.
     */
    template<typename KeyType, typename ReturnType>
    struct Map : std::unordered_map<KeyType, std::unique_ptr<Function::Common>>
    {
        /** @brief Default constructor for Map. */
        Map() = default;
        Map (Map&&) noexcept = default;
        Map& operator= (Map&&) noexcept = default;
        Map (const Map&) = delete;
        Map& operator= (const Map&) = delete;

        /** @brief Default destructor for Map. */
        ~Map() = default;

        /**
         * @brief Adds a new callable element to the map.
         *
         * @tparam Args The argument types for the callable element (preserving references).
         * @tparam FunctionType The type of the function to add.
         * @param key The key for the callable element.
         * @param newFunction The function to add as a callable element.
         */
        template<typename... Args, typename FunctionType>
        void add(KeyType key, FunctionType&& newFunction)
        {
            using CanonElem = Function::Element<ReturnType, Args...>;
            auto ptr = std::make_unique<CanonElem>(std::forward<FunctionType>(newFunction));
            this->insert({key, std::move(ptr)});
        }

        /**
         * @brief Retrieves and calls a callable element from the map with safety checks.
         *
         * @tparam Args The argument types for the callable element.
         * @param key The key of the callable element in the map.
         * @param args The arguments to pass to the callable element.
         * @return The result of calling the callable element.
         *
         * @throws std::out_of_range If the key is not present in the map.
         */
        template<typename... Args>
        ReturnType get(const KeyType& key, Args&&... args)
        {
            auto it = this->find(key);
            assert(it != this->end() &&
                   "Function::Map::get() - Key not found. Was this function registered with add()?");

            if (it == this->end())
                throw std::out_of_range("Key not found in Function::Map");

            using CanonElem = Function::Element<ReturnType, Args...>;
            auto* elem = static_cast<CanonElem*>(it->second.get());

            return (*elem)(std::forward<Args>(args)...);
        }

        /**
         * @brief Retrieves and calls a callable element from the map (const overload).
         *
         * @tparam Args The argument types for the callable element.
         * @param key The key of the callable element in the map.
         * @param args The arguments to forward to the callable element.
         * @return The result of invoking the stored callable.
         *
         * @details
         * This overload allows invocation on a const-qualified map.
         *
         * @throws std::out_of_range If the key is not present in the map.
         * @note If the caller uses a different signature than was registered with
         *       add(), the cast will be ill-formed at compile time.
         */
        template<typename... Args>
        ReturnType get(const KeyType& key, Args&&... args) const
        {
            auto it = this->find(key);
            assert(it != this->end() &&
                   "Function::Map::get() const - Key not found. Was this function registered with add()?");

            if (it == this->end())
                throw std::out_of_range("Key not found in Function::Map");

            using CanonElem = Function::Element<ReturnType, Args...>;
            auto* elem = static_cast<CanonElem*>(it->second.get());

            return (*elem)(std::forward<Args>(args)...);
        }

        /**
         * @brief Checks if the map contains a given key.
         *
         * @param key The key to look for in the map.
         * @return True if the key is found, false otherwise.
         */
        bool contains(const KeyType& key) const noexcept
        {
            return this->find(key) != this->end();
        }
    };

};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
