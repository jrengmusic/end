#pragma once

namespace jreng
{

template<typename SnapshotType>
class SnapshotBuffer
{
public:
    SnapshotBuffer() = default;
    ~SnapshotBuffer() = default;

    // MESSAGE THREAD
    SnapshotType& getWriteBuffer() noexcept
    {
        return snapshots.at (writeSlot);
    }

    // MESSAGE THREAD
    void write() noexcept
    {
        auto* returned { mailbox.write (&snapshots.at (writeSlot)) };

        if (returned == nullptr)
            writeSlot = (writeSlot == front) ? back : front;
    }

    // MESSAGE THREAD
    template<typename... Args>
    void resize (Args&&... args)
    {
        snapshots.at (front).resize (std::forward<Args> (args)...);
        snapshots.at (back).resize (std::forward<Args> (args)...);
    }

    bool isReady() const noexcept
    {
        return mailbox.isReady();
    }

    // RENDER THREAD
    SnapshotType* read() noexcept
    {
        if (auto* newest { mailbox.read() })
            readSnapshot = newest;

        return readSnapshot;
    }

private:
    enum Slot { front = 0, back = 1 };

    std::array<SnapshotType, 2> snapshots;
    Slot writeSlot { front };
    SnapshotType* readSnapshot { nullptr };
    Mailbox<SnapshotType> mailbox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SnapshotBuffer)
};

} // namespace jreng
