#ifndef DATASTRUCTURES_HPP
#define DATASTRUCTURES_HPP

#include <JuceHeader.h>

#include <algorithm>
#include <vector>

//==============================================================================
// Thread-safe single-slot "latest value wins" handoff. Use this instead of a
// multi-slot queue whenever only the most recently produced value matters
// and any older, superseded values can simply be discarded -- e.g. handing a
// newly-loaded audio source from a background thread to the thread that will
// consume it, where a file-load requested before the most recent one is
// irrelevant the moment a newer one arrives.
//
// Safe to call set()/getIfNew() from any thread(s) -- guarded by a SpinLock,
// so avoid calling from a real-time audio thread (nothing here does).
template <typename T>
class LatestValue {
   public:
    void set(T value) {
        const juce::SpinLock::ScopedLockType lock(mutex);
        current = std::move(value);
        available = true;
    }

    // Returns true and fills `out` if a value has been set() since the last
    // successful getIfNew(); otherwise returns false and leaves `out`
    // untouched.
    bool getIfNew(T& out) {
        const juce::SpinLock::ScopedLockType lock(mutex);
        if (!available)
            return false;

        out = current;
        available = false;
        return true;
    }

   private:
    juce::SpinLock mutex;
    T current{};
    bool available = false;
};

//==============================================================================
// Holds onto reference-counted objects until it's safe to let them be
// destroyed -- i.e. until nothing else still holds a reference (in
// particular, until the audio thread has stopped using them). add() may be
// called from any thread; pruning happens on this object's own Timer
// callback (message thread), where deletion is safe.
template <typename ReferenceCountedType>
struct ReleasePool : juce::Timer {
    ReleasePool() {
        deletionPool.reserve(5000);
        startTimer(1 * 1000);
    }

    ~ReleasePool() override {
        stopTimer();
    }

    using Ptr = typename ReferenceCountedType::Ptr;

    // Safe to call from any thread.
    void add(Ptr ptr) {
        if (ptr == nullptr)
            return;

        const juce::SpinLock::ScopedLockType lock(mutex);
        addIfNotAlreadyThere(ptr);
    }

    void timerCallback() override {
        const juce::SpinLock::ScopedLockType lock(mutex);
        deletionPool.erase(
            std::remove_if(
                deletionPool.begin(),
                deletionPool.end(),
                [](const auto& ptr) { return ptr->getReferenceCount() <= 1; }),
            deletionPool.end());
    }

   private:
    juce::SpinLock mutex;
    std::vector<Ptr> deletionPool;

    // Called under `mutex`.
    void addIfNotAlreadyThere(Ptr ptr) {
        auto found = std::find_if(
            deletionPool.begin(), deletionPool.end(), [ptr](const auto& elem) {
                return elem.get() == ptr.get();
            });

        if (found == deletionPool.end())
            deletionPool.push_back(ptr);
    }
};

#endif /* DATASTRUCTURES_HPP */