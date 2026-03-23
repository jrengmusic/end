/**
 * @file jreng_lru_glyph_cache.h
 * @brief Fixed-capacity glyph cache with least-recently-used eviction.
 */

#pragma once

#include <unordered_map>

namespace jreng::Glyph
{

/**
 * @class LRUCache
 * @brief Fixed-capacity glyph cache with least-recently-used eviction.
 *
 * Wraps an `std::unordered_map<Key, CacheEntry>` and tracks a monotonic
 * frame counter.  On each cache hit, `lastAccessFrame` is updated to the
 * current frame.  When the map reaches `capacityLimit`, `evictLRU()` removes
 * the oldest 10 % of entries (those with the largest `currentFrame -
 * lastAccessFrame` delta).
 *
 * @par Eviction strategy
 * `evictLRU()` builds a temporary age list on the heap, partial-sorts it to
 * find the `targetRemove` oldest entries, then erases them from the map.  This
 * avoids allocating a sorted copy of the entire map on every eviction.
 *
 * @par Thread context
 * **MESSAGE THREAD** — not thread-safe.  All methods must be called from the
 * same thread.
 *
 * @see GlyphAtlas
 * @see Key
 * @see Region
 */
// MESSAGE THREAD - NOT thread-safe
class LRUCache
{
public:
    /**
     * @struct CacheEntry
     * @brief Internal storage for one cached glyph.
     */
    struct CacheEntry
    {
        /** @brief The rasterized glyph descriptor. */
        Region glyph;

        /**
         * @brief Frame number of the most recent access.
         *
         * Updated by `get()` on every cache hit.  Used by `evictLRU()` to
         * compute entry age as `currentFrame - lastAccessFrame`.
         */
        uint64_t lastAccessFrame { 0 };
    };

    /**
     * @brief Construct a cache with the given capacity limit.
     * @param capacityLimit Maximum number of entries before eviction triggers.
     *        The internal map is pre-reserved to half this value.
     */
    explicit LRUCache (uint32_t capacityLimit) noexcept
        : capacityLimit (capacityLimit)
    {
        cache.reserve (capacityLimit / 2);
    }

    /**
     * @brief Look up a glyph by key, updating its access frame on hit.
     * @param key The glyph identity to look up.
     * @return Pointer to the cached `Region`, or `nullptr` on miss.
     * @note The returned pointer is valid until the next `insert()` or
     *       `clear()` call (which may rehash the map).
     */
    Region* get (const Key& key) noexcept
    {
        auto it { cache.find (key) };
        Region* result { nullptr };

        if (it != cache.end())
        {
            it->second.lastAccessFrame = currentFrame;
            result = &it->second.glyph;
        }

        return result;
    }

    /**
     * @brief Insert a new glyph into the cache, evicting LRU entries if full.
     *
     * If the cache is at capacity, `evictLRU()` is called first to free space.
     * The glyph is then emplaced with `currentFrame` as its access time.
     *
     * @param key   The glyph identity (must not already be present).
     * @param glyph The rasterized glyph descriptor to store.
     * @return Pointer to the newly inserted `Region`, or `nullptr` if
     *         insertion failed (duplicate key).
     */
    Region* insert (const Key& key, const Region& glyph) noexcept
    {
        if (cache.size() >= capacityLimit)
        {
            evictLRU();
        }

        auto [it, success] = cache.emplace (
            key,
            CacheEntry { glyph, currentFrame }
        );

        Region* result { nullptr };

        if (success)
        {
            result = &it->second.glyph;
        }

        return result;
    }

    /**
     * @brief Advance the frame counter by one.
     *
     * Must be called once per rendered frame so that LRU age calculations
     * remain accurate.  Typically called from `GlyphAtlas::advanceFrame()`.
     */
    void advanceFrame() noexcept
    {
        ++currentFrame;
    }

    /**
     * @brief Remove all entries and reset the frame counter to zero.
     *
     * Called when the atlas texture is invalidated (e.g. on font size change
     * or GL context loss).
     */
    void clear() noexcept
    {
        cache.clear();
        currentFrame = 0;
    }

    /** @brief Current number of entries in the cache. */
    size_t getSize() const noexcept { return cache.size(); }

    /** @brief Maximum number of entries before eviction triggers. */
    size_t getCapacity() const noexcept { return capacityLimit; }

    /** @brief Monotonic frame counter; incremented by `advanceFrame()`. */
    uint64_t getCurrentFrame() const noexcept { return currentFrame; }

private:
    /** @brief Primary storage: glyph key → cache entry. */
    std::unordered_map<Key, CacheEntry> cache;

    /** @brief Maximum number of entries before LRU eviction triggers. */
    uint32_t capacityLimit { 0 };

    /** @brief Monotonic frame counter used to compute entry age. */
    uint64_t currentFrame { 0 };

    /**
     * @struct AgeEntry
     * @brief Temporary record used during LRU eviction sorting.
     */
    struct AgeEntry
    {
        /** @brief Key of the cache entry. */
        Key key;

        /** @brief Age in frames: `currentFrame - lastAccessFrame`. */
        uint64_t age;
    };

    /**
     * @brief Build a flat age list from the current cache contents.
     *
     * Allocates a `HeapBlock<AgeEntry>` of `cache.size()` elements and
     * populates it with each entry's key and computed age.
     *
     * @param ageList Output block; allocated inside this function.
     * @return Number of entries written (equals `cache.size()`).
     */
    int buildAgeList (juce::HeapBlock<AgeEntry>& ageList) const noexcept
    {
        const int count { static_cast<int> (cache.size()) };
        ageList.allocate (static_cast<size_t> (count), false);

        int idx { 0 };
        for (const auto& [key, entry] : cache)
        {
            jassert (idx >= 0 and idx < count);
            ageList[idx].key = key;
            ageList[idx].age = currentFrame - entry.lastAccessFrame;
            ++idx;
        }

        return count;
    }

    /**
     * @brief Evict the oldest 10 % of cache entries.
     *
     * Uses `std::partial_sort` on a temporary age list to identify the
     * `targetRemove` (= max(1, capacity/10)) oldest entries, then erases them
     * from the map.  This keeps the eviction cost proportional to the number
     * of entries removed rather than the full cache size.
     */
    void evictLRU() noexcept
    {
        if (not cache.empty())
        {
            const uint32_t targetRemove { std::max (1u, capacityLimit / 10) };
            juce::HeapBlock<AgeEntry> ageList;
            const int ageCount { buildAgeList (ageList) };

            std::partial_sort (
                ageList.get(),
                ageList.get() + std::min (static_cast<size_t> (ageCount), static_cast<size_t> (targetRemove)),
                ageList.get() + ageCount,
                [] (const AgeEntry& a, const AgeEntry& b) { return a.age > b.age; }
            );

            const int removeCount { static_cast<int> (std::min (static_cast<size_t> (targetRemove), static_cast<size_t> (ageCount))) };

            for (int i { 0 }; i < removeCount; ++i)
            {
                jassert (i >= 0 and i < removeCount);
                cache.erase (ageList[i].key);
            }
        }
    }
};

} // namespace jreng::Glyph
