#pragma once

namespace jreng
{

template<typename SnapshotType>
class Mailbox
{
public:
    Mailbox() = default;
    ~Mailbox() = default;

    // MESSAGE THREAD
    SnapshotType* write (SnapshotType* latest) noexcept
    {
        return slot.exchange (latest, std::memory_order_acq_rel);
    }

    // RENDER THREAD
    SnapshotType* read() noexcept
    {
        return slot.exchange (nullptr, std::memory_order_acq_rel);
    }

    bool isReady() const noexcept
    {
        return slot.load (std::memory_order_acquire) != nullptr;
    }

private:
    std::atomic<SnapshotType*> slot { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Mailbox)
};

} // namespace jreng
