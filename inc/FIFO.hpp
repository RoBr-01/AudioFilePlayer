#ifndef FIFO_HPP
#define FIFO_HPP

#include <JuceHeader.h>

#include <cstddef>

template <typename T, size_t Size = 30>
class Fifo {
   public:
    size_t getSize() const noexcept {
        return Size;
    }

    bool push(const T& t) {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0) {
            size_t index = static_cast<size_t>(write.startIndex1);
            buffer[index] = t;
            return true;
        }

        return false;
    }

    bool pull(T& t) {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0) {
            t = buffer[static_cast<size_t>(read.startIndex1)];
            return true;
        }

        return false;
    }

    int getNumAvailableForReading() const {
        return fifo.getNumReady();
    }

    int getAvailableSpace() const {
        return fifo.getFreeSpace();
    }

   private:
    juce::AbstractFifo fifo{Size};
    std::array<T, Size> buffer;
};

#endif /* FIFO_HPP */