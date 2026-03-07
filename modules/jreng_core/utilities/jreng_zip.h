/**
 
 https://codereview.stackexchange.com/questions/239437/zip-operator-to-iterate-on-multiple-container-in-a-sign
 ______________________________________________________________________________*/

#pragma once
#include <exception>
#include <iterator>
#include <tuple>

namespace jreng
{
/*____________________________________________________________________________*/

namespace detail
{
/*____________________________________________________________________________*/

using std::begin, std::end;

/**
 * @brief Traits for determining iterator and value types of a given range.
 *
 * This struct extracts key type information from a given range,
 * including its iterator type, value type, and reference type.
 *
 * @tparam Range The range type.
 */
template <typename Range>
struct range_traits
{
    using iterator = decltype (begin (std::declval<Range>()));
    using value_type = typename std::iterator_traits<iterator>::value_type;
    using reference = typename std::iterator_traits<iterator>::reference;
};

/**
 * @brief A zip iterator for iterating over multiple sequences in parallel.
 *
 * This iterator enables simultaneous traversal of multiple sequences,
 * producing a tuple of references to corresponding elements at each step.
 *
 * @tparam Its Variadic template for iterator types.
 */
template <typename... Its>
class zip_iterator
{
public:
    /// Common iterator category among provided iterators.
    using iterator_category = std::common_type_t<typename std::iterator_traits<Its>::iterator_category...>;
    /// Common difference type among provided iterators.
    using difference_type = std::common_type_t<typename std::iterator_traits<Its>::difference_type...>;
    /// Tuple containing value types from each iterator.
    using value_type = std::tuple<typename std::iterator_traits<Its>::value_type...>;
    /// Tuple containing references to values pointed to by iterators.
    using reference = std::tuple<typename std::iterator_traits<Its>::reference...>;
    /// Tuple containing pointer types of the iterators.
    using pointer = std::tuple<typename std::iterator_traits<Its>::pointer...>;

    /**
     * @brief Default constructor.
     */
    constexpr zip_iterator() = default;

    /**
     * @brief Constructs a zip iterator from multiple iterators.
     *
     * @param its The iterators to zip together.
     */
    explicit constexpr zip_iterator(Its... its) : base_its{its...} {}

    /**
     * @brief Dereferences the iterator to return a tuple of references.
     *
     * @return A tuple containing references to the values pointed to by the iterators.
     */
    constexpr reference operator*() const
    {
        return std::apply([](auto&... its) { return reference(*its...); }, base_its);
    }

    /**
     * @brief Advances the iterator (prefix increment).
     *
     * @return Reference to this iterator after advancement.
     */
    constexpr zip_iterator& operator++()
    {
        std::apply([](auto&... its) { (++its, ...); }, base_its);
        return *this;
    }

    /**
     * @brief Advances the iterator (postfix increment).
     *
     * @return A copy of the iterator before advancement.
     */
    constexpr zip_iterator operator++(int)
    {
        return std::apply([](auto&... its) { return zip_iterator(its++...); }, base_its);
    }

    /**
     * @brief Decrements the iterator (prefix decrement).
     *
     * @return Reference to this iterator after decrement.
     */
    constexpr zip_iterator& operator--()
    {
        std::apply([](auto&... its) { (--its, ...); }, base_its);
        return *this;
    }

    /**
     * @brief Decrements the iterator (postfix decrement).
     *
     * @return A copy of the iterator before decrement.
     */
    constexpr zip_iterator operator--(int)
    {
        return std::apply([](auto&... its) { return zip_iterator(its--...); }, base_its);
    }

    /**
     * @brief Advances iterator by a given offset.
     *
     * @param n Offset amount.
     * @return Reference to this iterator after advancement.
     */
    constexpr zip_iterator& operator+=(difference_type n)
    {
        std::apply([=](auto&... its) { ((its += n), ...); }, base_its);
        return *this;
    }

    /**
     * @brief Moves iterator backwards by a given offset.
     *
     * @param n Offset amount.
     * @return Reference to this iterator after movement.
     */
    constexpr zip_iterator& operator-=(difference_type n)
    {
        std::apply([=](auto&... its) { ((its -= n), ...); }, base_its);
        return *this;
    }
    
    friend constexpr zip_iterator operator+ (const zip_iterator& it,
                                             difference_type n)
    {
        return std::apply ([=] (auto&... its)
                           { return zip_iterator(its + n...); }, it.base_its);
    }
    
    friend constexpr zip_iterator operator+ (difference_type n,
                                             const zip_iterator& it)
    {
        return std::apply ([=] (auto&... its)
                           { return zip_iterator(n + its...); }, it.base_its);
    }
    
    friend constexpr zip_iterator operator- (const zip_iterator& it,
                                            difference_type n)
    {
        return std::apply ([=] (auto&... its)
                           { return zip_iterator(its - n...); }, it.base_its);
    }
    
    constexpr reference operator[](difference_type n) const
    {
        return std::apply ([=] (auto&... its)
                           { return reference(its[n]...); }, base_its);
    }

    // the following functions assume usual random access iterator semantics
    /**
     * @brief Equality comparison operator for zip_iterator.
     *
     * Determines if two zip_iterators are equal by comparing the first iterator
     * in their respective tuple of base iterators.
     *
     * @param lhs Left-hand side zip_iterator.
     * @param rhs Right-hand side zip_iterator.
     * @return True if the first iterator in lhs equals the first iterator in rhs, false otherwise.
     */
    friend constexpr bool operator== (const zip_iterator& lhs, const zip_iterator& rhs)
    {
        return std::get<0>(lhs.base_its) == std::get<0>(rhs.base_its);
    }

    /**
     * @brief Inequality comparison operator for zip_iterator.
     *
     * Determines if two zip_iterators are not equal.
     *
     * @param lhs Left-hand side zip_iterator.
     * @param rhs Right-hand side zip_iterator.
     * @return True if lhs is not equal to rhs, false otherwise.
     */
    friend constexpr bool operator!= (const zip_iterator& lhs, const zip_iterator& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Less-than comparison operator for zip_iterator.
     *
     * Compares two zip_iterators based on the first iterator in their tuple.
     *
     * @param lhs Left-hand side zip_iterator.
     * @param rhs Right-hand side zip_iterator.
     * @return True if lhs is less than rhs, false otherwise.
     */
    friend constexpr bool operator< (const zip_iterator& lhs, const zip_iterator& rhs)
    {
        return std::get<0>(lhs.base_its) < std::get<0>(rhs.base_its);
    }

    /**
     * @brief Greater-than comparison operator for zip_iterator.
     *
     * Determines if one zip_iterator is greater than another.
     *
     * @param lhs Left-hand side zip_iterator.
     * @param rhs Right-hand side zip_iterator.
     * @return True if lhs is greater than rhs, false otherwise.
     */
    friend constexpr bool operator> (const zip_iterator& lhs, const zip_iterator& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Less-than-or-equal comparison operator for zip_iterator.
     *
     * Determines if one zip_iterator is less than or equal to another.
     *
     * @param lhs Left-hand side zip_iterator.
     * @param rhs Right-hand side zip_iterator.
     * @return True if lhs is less than or equal to rhs, false otherwise.
     */
    friend constexpr bool operator<= (const zip_iterator& lhs, const zip_iterator& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Greater-than-or-equal comparison operator for zip_iterator.
     *
     * Determines if one zip_iterator is greater than or equal to another.
     *
     * @param lhs Left-hand side zip_iterator.
     * @param rhs Right-hand side zip_iterator.
     * @return True if lhs is greater than or equal to rhs, false otherwise.
     */
    friend constexpr bool operator>= (const zip_iterator& lhs, const zip_iterator& rhs)
    {
        return !(lhs < rhs);
    }

    
private:
    /// Stores the iterators being zipped.
    std::tuple<Its...> base_its;
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace detail */

/**
 * @brief A zip range adapter that allows iteration over multiple ranges in parallel.
 *
 * This class enables simultaneous traversal of multiple ranges, returning
 * a tuple of elements from each range at each step.
 *
 * @tparam Ranges The range types to be zipped.
 */
template <typename... Ranges>
class zip
{
    static_assert (sizeof...(Ranges) > 0, "Cannot zip empty ranges");
public:
    /// Iterator type for this zipped range.
    using iterator = detail::zip_iterator<typename detail::range_traits<Ranges>::iterator...>;
    /// Value type returned by the iterator.
    using value_type = typename iterator::value_type;
    /// Reference type returned by the iterator.
    using reference = typename iterator::reference;

    /**
     * @brief Constructs a zip object from multiple ranges.
     *
     * @param rs The ranges to zip together.
     */
    explicit constexpr zip(Ranges&&... rs) : ranges{std::forward<Ranges>(rs)...} {}

    /**
     * @brief Returns an iterator pointing to the beginning of the zipped ranges.
     *
     * @return Iterator pointing to the start of the zipped sequence.
     */
    constexpr iterator begin()
    {
        return std::apply([](auto&... rs) { return iterator(rs.begin()...); }, ranges);
    }

    /**
     * @brief Returns an iterator pointing to the end of the zipped ranges.
     *
     * @return Iterator pointing to the end of the zipped sequence.
     */
    constexpr iterator end()
    {
        return std::apply([](auto&... rs) { return iterator(rs.end()...); }, ranges);
    }
    
private:
    /// Stores the ranges being zipped.
    std::tuple<Ranges...> ranges;
};

// by default, rvalue arguments are moved to prevent dangling references
// Deduction guide to infer types of ranges.

/**
 * @brief Example usage of the zip function.
 *
 * This demonstrates how to iterate over multiple containers in parallel
 * using structured bindings and perfect forwarding (`const auto&&`).
 *
 * @code
 * std::vector<int> vec1 = {1, 2, 3};
 * std::vector<char> vec2 = {'A', 'B', 'C'};
 *
 * for (const auto&& [num, letter] : zip(vec1, vec2))
 * {
 *     std::cout << num << " - " << letter << std::endl;
 * }
 * @endcode
 *
 * The use of `const auto&&` ensures that elements are efficiently forwarded
 * while still allowing modifications if the original range permits it.
 */
template <typename... Ranges>
explicit zip (Ranges&&...) -> zip<Ranges...>;

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
