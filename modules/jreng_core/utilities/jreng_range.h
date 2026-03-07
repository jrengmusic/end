/** -----------------------------------------------------------------------------------------
    https://github.com/klmr/cpp11-range
    -----------------------------------------------------------------------------------------
 
 ## Re-imagining the `for` loop

         for (auto i : range (1, 5))
             cout << i << "\n";

         for (auto u : range (0u))
             if (u == 3u) break;
             else         cout << u << "\n";

         for (auto c : range('a', 'd'))
             cout << c << "\n";

         for (auto i : range (100).step(-3))
             if (i < 90) break;
             else        cout << i << "\n";

 **☞ Beauty is free.**
------------------------------------------------------------------------------------------- */

#pragma once
#include <cmath>
#include <iterator>
#include <type_traits>

namespace jreng
{
/*____________________________________________________________________________*/
namespace detail
{
/*____________________________________________________________________________*/

/**
 * @brief Base iterator class for range-based iteration.
 *
 * @tparam T The type of value being iterated over.
 */
template<typename T>
struct range_iter_base
{
    /**
     * @brief Constructs an iterator with an initial value.
     *
     * @param current The initial value.
     */
    range_iter_base (T newCurrent)
        : current (newCurrent) {}

    /**
     * @brief Dereferences the iterator.
     *
     * @return The current value.
     */
    T operator*() const { return current; }

    /**
     * @brief Provides access to the current value.
     *
     * @return Pointer to the current value.
     */
    T const* operator->() const { return &current; }

    /**
     * @brief Advances the iterator (prefix increment).
     *
     * @return Reference to this iterator after increment.
     */
    range_iter_base& operator++()
    {
        ++current;
        return *this;
    }

    /**
     * @brief Advances the iterator (postfix increment).
     *
     * @return A copy of the iterator before increment.
     */
    range_iter_base operator++ (int)
    {
        auto copy = *this;
        ++*this;
        return copy;
    }

    /**
     * @brief Equality comparison.
     *
     * @param other The iterator to compare with.
     * @return True if equal, false otherwise.
     */
    bool operator== (range_iter_base const& other) const { return current == other.current; }

    /**
     * @brief Inequality comparison.
     *
     * @param other The iterator to compare with.
     * @return True if not equal, false otherwise.
     */
    bool operator!= (range_iter_base const& other) const { return ! (*this == other); }

protected:
    T current; /**< Stores the current iteration value. */
};

/*____________________________________________________________________________*/
}// namespace detail

/**
 * @brief A proxy for generating a stepped range of values.
 *
 * This template class allows iteration over a range with a specified step size.
 * It provides bidirectional iteration support and handles empty ranges gracefully.
 *
 * @tparam T The numeric type used for the range.
 */
template<typename T>
struct step_range_proxy
{
    /**
     * @brief Iterator for step_range_proxy.
     *
     * This iterator moves through the range in specified step increments.
     */
    struct iterator : detail::range_iter_base<T>
    {
        /**
         * @brief Constructs an iterator with the given starting position and step size.
         *
         * @param current The current position of the iterator.
         * @param step The step increment for iteration.
         */
        iterator (T current, T step)
            : detail::range_iter_base<T> (current), step_ (step) {}

        using detail::range_iter_base<T>::current;

        /**
         * @brief Advances the iterator by one step (prefix).
         * @return Reference to the incremented iterator.
         */
        iterator& operator++()
        {
            current += step_;
            return *this;
        }

        /**
         * @brief Advances the iterator by one step (postfix).
         * @return Copy of the iterator before incrementing.
         */
        iterator operator++ (int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }

        /**
         * @brief Compares two iterators for equality.
         *
         * Iterators lose commutativity due to step direction constraints.
         * @param other The iterator to compare against.
         * @return True if the iterators are considered equal.
         */
        bool operator== (iterator const& other) const
        {
            return step_ > 0 ? current >= other.current
                             : current < other.current;
        }

        /**
         * @brief Compares two iterators for inequality.
         * @param other The iterator to compare against.
         * @return True if the iterators are not equal.
         */
        bool operator!= (iterator const& other) const
        {
            return not (*this == other);
        }

        T step_;///< The step size for iteration.
    };

    /**
     * @brief Constructs a stepped range from `begin` to `end` with the given `step`.
     * @param begin The starting value of the range.
     * @param end The ending value of the range.
     * @param step The step increment.
     */
    step_range_proxy (T begin, T end, T step)
        : begin_ (begin, step), end_ (end, step) {}

    /**
     * @brief Returns an iterator pointing to the beginning of the range.
     * @return Iterator at the start of the range.
     */
    iterator begin() const { return begin_; }

    /**
     * @brief Returns an iterator pointing to the end of the range.
     * @return Iterator at the end of the range.
     */
    iterator end() const { return end_; }

    /**
     * @brief Computes the number of steps needed to traverse the range.
     *
     * Handles both increasing and decreasing ranges and prevents invalid step sizes.
     * @return The number of steps in the range.
     */
    std::size_t size() const
    {
        if (*end_ >= *begin_)
        {
            // Increasing and empty range
            if (begin_.step_ < T { 0 })
                return 0;
        }
        else
        {
            // Decreasing range
            if (begin_.step_ > T { 0 })
                return 0;
        }
        return std::ceil (std::abs (static_cast<double> (*end_ - *begin_) / begin_.step_));
    }

private:
    iterator begin_;///< Iterator marking the beginning of the range.
    iterator end_;///< Iterator marking the end of the range.
};

/**
 * @brief A proxy for generating a range of values.
 *
 * This template class provides iteration over a range without step control.
 * A step size can be defined via `step_range_proxy`.
 *
 * @tparam T The numeric type used for the range.
 */
template<typename T>
struct range_proxy
{
    /**
     * @brief Iterator for range_proxy.
     *
     * Moves through the range incrementally.
     */
    struct iterator : detail::range_iter_base<T>
    {
        /**
         * @brief Constructs an iterator with the given starting position.
         *
         * @param current The current position of the iterator.
         */
        iterator (T current)
            : detail::range_iter_base<T> (current) {}
    };

    /**
     * @brief Constructs a range from `begin` to `end`.
     * @param begin The starting value of the range.
     * @param end The ending value of the range.
     */
    range_proxy (T begin, T end)
        : begin_ (begin), end_ (end) {}

    /**
     * @brief Converts this range into a stepped range.
     * @param step The step increment.
     * @return A `step_range_proxy` object.
     */
    step_range_proxy<T> step (T step)
    {
        return { *begin_, *end_, step };
    }

    /**
     * @brief Returns an iterator pointing to the beginning of the range.
     * @return Iterator at the start of the range.
     */
    iterator begin() const { return begin_; }

    /**
     * @brief Returns an iterator pointing to the end of the range.
     * @return Iterator at the end of the range.
     */
    iterator end() const { return end_; }

    /**
     * @brief Computes the number of elements in the range.
     * @return The number of elements in the range.
     */
    std::size_t size() const { return *end_ - *begin_; }

private:
    iterator begin_;///< Iterator marking the beginning of the range.
    iterator end_;///< Iterator marking the end of the range.
};

/**
 * @brief A proxy for generating an infinite stepped range.
 *
 * This template class provides continuous iteration with a step size.
 *
 * @tparam T The numeric type used for the range.
 */
template<typename T>
struct step_inf_range_proxy
{
    /**
     * @brief Iterator for step_inf_range_proxy.
     *
     * Moves through the range continuously in step increments.
     */
    struct iterator : detail::range_iter_base<T>
    {
        /**
         * @brief Constructs an iterator with optional starting position and step.
         * @param current The current position of the iterator.
         * @param step The step increment.
         */
        iterator (T current = T(), T step = T())
            : detail::range_iter_base<T> (current), step (step) {}

        using detail::range_iter_base<T>::current;

        /**
         * @brief Advances the iterator by one step (prefix).
         * @return Reference to the incremented iterator.
         */
        iterator& operator++()
        {
            current += step;
            return *this;
        }

        /**
         * @brief Advances the iterator by one step (postfix).
         * @return Copy of the iterator before incrementing.
         */
        iterator operator++ (int)
        {
            auto copy = *this;
            ++*this;
            return copy;
        }

        /**
         * @brief Infinite iterator always evaluates as unequal.
         * @return False (always).
         */
        bool operator== (iterator const&) const { return false; }

        /**
         * @brief Infinite iterator always evaluates as unequal.
         * @return True (always).
         */
        bool operator!= (iterator const&) const { return true; }

    private:
        T step;///< Step increment for iteration.
    };

    /**
     * @brief Constructs an infinite stepped range starting at `begin` with step `step`.
     * @param begin The starting value of the range.
     * @param step The step increment.
     */
    step_inf_range_proxy (T begin, T step)
        : begin_ (begin, step) {}

    /**
     * @brief Returns an iterator pointing to the beginning of the range.
     * @return Iterator at the start of the range.
     */
    iterator begin() const { return begin_; }

    /**
     * @brief Returns an iterator marking an infinite range.
     * @return An iterator representing infinity.
     */
    iterator end() const { return iterator(); }

private:
    iterator begin_;///< Iterator marking the beginning of the range.
};

/**
 * @brief A proxy for generating an infinite range.
 *
 * This template class provides continuous iteration without a step size.
 * A step size can be applied via `step_inf_range_proxy`.
 *
 * @tparam T The numeric type used for the range.
 */
template<typename T>
struct infinite_range_proxy
{
    /**
     * @brief Iterator for infinite_range_proxy.
     *
     * Moves through the range continuously without step control.
     */
    struct iterator : detail::range_iter_base<T>
    {
        /**
         * @brief Constructs an iterator with optional starting position.
         * @param current The current position of the iterator.
         */
        iterator (T current = T())
            : detail::range_iter_base<T> (current) {}

        /**
         * @brief Infinite iterator always evaluates as unequal.
         * @return False (always).
         */
        bool operator== (iterator const&) const { return false; }

        /**
         * @brief Infinite iterator always evaluates as unequal.
         * @return True (always).
         */
        bool operator!= (iterator const&) const { return true; }
    };

    /**
     * @brief Constructs an infinite range starting at `begin`.
     * @param begin The starting value of the range.
     */
    infinite_range_proxy (T begin)
        : begin_ (begin) {}

    /**
     * @brief Converts this range into an infinite stepped range.
     * @param step The step increment.
     * @return A `step_inf_range_proxy` object.
     */
    step_inf_range_proxy<T> step (T step)
    {
        return { *begin_, step };
    }

    /**
     * @brief Returns an iterator pointing to the beginning of the range.
     * @return Iterator at the start of the range.
     */
    iterator begin() const { return begin_; }

    /**
     * @brief Returns an iterator marking an infinite range.
     * @return An iterator representing infinity.
     */
    iterator end() const { return iterator(); }

private:
    iterator begin_;///< Iterator marking the beginning of the range.
};

/**
 * @brief Creates a range with a specified beginning and end.
 *
 * This function template returns a `range_proxy` object representing a numeric range
 * between `begin` and `end`. The common type between `T` and `U` is determined and
 * used for consistency.
 *
 * @tparam T The type of the beginning value.
 * @tparam U The type of the ending value.
 * @param begin The starting value of the range.
 * @param end The ending value of the range.
 * @return A `range_proxy` object representing the specified range.
 *
 * @code
 * // Iterates from 1 to 4 (inclusive).
 * for (auto i : range (1, 5))
 *     std::cout << i << "\n";
 *
 * // Iterates from 0 until 3, breaking when `u == 3`.
 * for (auto u : range (0u))
 *     if (u == 3u) break;
 *     else         std::cout << u << "\n";
 *
 * // Iterates from 'a' to 'c'.
 * for (auto c : range('a', 'd'))
 *     std::cout << c << "\n";
 *
 * // Iterates down from 100, stepping by -3.
 * for (auto i : range (100).step(-3))
 *     if (i < 90) break;
 *     else        std::cout << i << "\n";
 * @endcode
 */
template<typename T, typename U>
auto range (T begin, U end) -> range_proxy<typename std::common_type<T, U>::type>
{
    using C = typename std::common_type<T, U>::type;
    return { static_cast<C> (begin), static_cast<C> (end) };
}

/**
 * @brief Creates an infinite range starting at `begin`.
 *
 * This function template returns an `infinite_range_proxy` object representing a
 * continuous range starting from the given value.
 *
 * @tparam T The type of the starting value.
 * @param begin The starting value of the infinite range.
 * @return An `infinite_range_proxy` object representing an infinite range.
 */
template<typename T>
infinite_range_proxy<T> range (T begin)
{
    return { begin };
}

namespace traits
{
/*____________________________________________________________________________*/

/**
 * @brief Trait to check if a type has a `size()` member function returning an integral type.
 *
 * This template struct determines whether a given type `C` has a member function `size()`
 * that returns an integral type. If such a function exists, `value` is `true`; otherwise, it is `false`.
 *
 * @tparam C The type to check.
 */
template<typename C>
struct has_size
{
    /**
     * @brief Checks if a type `T` has a `size()` member function returning an integral type.
     *
     * Uses SFINAE to detect the presence of `size()` returning an integral type.
     * @tparam T The type being checked.
     * @return `std::true_type` if `size()` exists and returns an integral type; otherwise, `std::false_type`.
     */
    template<typename T>
    static auto check (T*) ->
        typename std::is_integral<
            decltype (std::declval<T const>().size())>::type;

    /**
     * @brief Fallback check for types without `size()`.
     * @return `std::false_type` indicating the absence of an appropriate `size()` function.
     */
    template<typename>
    static auto check (...) -> std::false_type;

    /**
     * @brief Alias representing the result type of the check.
     */
    using type = decltype (check<C> (0));

    /**
     * @brief Boolean value indicating whether the type has a `size()` function returning an integral type.
     */
    static constexpr bool value = type::value;
};

/*____________________________________________________________________________*/
}// namespace traits

/**
 * @brief Generates an index range for a container supporting `size()`.
 *
 * This function template returns a `range_proxy` object representing indices
 * from `0` to `cont.size()`. It uses `std::enable_if` to ensure compatibility with containers
 * that have a `size()` member function.
 *
 * @tparam C The container type.
 * @param cont The container whose indices are to be generated.
 * @return A `range_proxy` representing valid indices.
 *
 * @code
 * std::vector<int> v = {10, 20, 30, 40};
 * for (auto i : indices(v))
 *     std::cout << i << "\n"; // Outputs: 0 1 2 3
 * @endcode
 */
template<typename C, typename = typename std::enable_if<traits::has_size<C>::value>>
auto indices (C const& cont) -> range_proxy<decltype (cont.size())>
{
    return { 0, cont.size() };
}

/**
 * @brief Generates an index range for a raw array.
 *
 * This function template returns a `range_proxy` representing indices from `0` to `N`.
 *
 * @tparam T The type of elements in the array.
 * @tparam SizeType The type representing the size of the array.
 * @tparam N The number of elements in the array.
 * @param array The raw array.
 * @return A `range_proxy` representing valid indices.
 *
 * @code
 * int arr[5] = {1, 2, 3, 4, 5};
 * for (auto i : indices(arr))
 *     std::cout << i << "\n"; // Outputs: 0 1 2 3 4
 * @endcode
 */
template<typename T, typename SizeType, SizeType N>
range_proxy<SizeType> indices (T (&array)[N])
{
    return { 0, N };
}

/**
 * @brief Generates an index range for an initializer list.
 *
 * This function template returns a `range_proxy` representing indices from `0` to `cont.size()`.
 *
 * @tparam T The type of elements in the initializer list.
 * @param cont The initializer list whose indices are to be generated.
 * @return A `range_proxy` representing valid indices.
 *
 * @code
 * for (auto i : indices({10, 20, 30, 40}))
 *     std::cout << i << "\n"; // Outputs: 0 1 2 3
 * @endcode
 */
template<typename T>
range_proxy<typename std::initializer_list<T>::size_type>
indices (std::initializer_list<T>&& cont)
{
    return { 0, cont.size() };
}

/**____________________________________END OF NAMESPACE_____________________________________*/
} /** namespace jreng */
