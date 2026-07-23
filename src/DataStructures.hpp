#ifndef DATASTRUCTURES_HPP
#define DATASTRUCTURES_HPP

#include <JuceHeader.h>

#include <algorithm>
#include <vector>

template <typename T>
class LatestValue {
   public:
    void set(T value) {
        const juce::SpinLock::ScopedLockType lock(mutex);
        current = std::move(value);
        available = true;
    }

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

#endif/* DATASTRUCTURES_HPP */
