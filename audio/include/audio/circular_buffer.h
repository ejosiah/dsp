#pragma once

#include <cstdint>
#include <atomic>
#include <vector>

namespace audio {

    template<typename SampleType>
    class CircularAudioBuffer{
    public:
        CircularAudioBuffer();

        explicit CircularAudioBuffer(uint32_t capacity);

        void capacity(uint32_t value);

        uint32_t capacity() const;

        uint32_t remainder() const;

        uint32_t push(const SampleType* iBuffer, uint32_t numSamples);

        uint32_t peek(SampleType* oBuffer, uint32_t numSamples) const;

        uint32_t pop(SampleType* outBuffer, uint32_t numSamples);

        void setNum(uint32_t numSamples, bool retainOldestSamples = false);

        uint32_t num() const;

    private:
        std::vector<SampleType> m_buffer;
        uint32_t m_capacity;
        std::atomic_uint32_t m_readCounter;
        std::atomic_uint32_t m_writeCounter;
    };

    template<typename SampleType>
    CircularAudioBuffer<SampleType>::CircularAudioBuffer() {
        capacity(0);
    }

    template<typename SampleType>
    CircularAudioBuffer<SampleType>::CircularAudioBuffer(uint32_t inCapacity) {
        capacity(inCapacity);
    }

    template<typename SampleType>
    void CircularAudioBuffer<SampleType>::capacity(uint32_t value) {
        m_capacity = value + 1;
        m_readCounter = 0;
        m_writeCounter = 0;
        m_buffer.resize(m_capacity);
    }

    template<typename SampleType>
    uint32_t CircularAudioBuffer<SampleType>::capacity() const {
        return m_capacity;
    }

    template<typename SampleType>
    uint32_t CircularAudioBuffer<SampleType>::remainder() const {
        const auto readIndex = m_readCounter.load();
        const auto writeIndex = m_writeCounter.load();
        return (m_capacity - 1 - writeIndex + readIndex) % m_capacity;
    }

    template<typename SampleType>
    uint32_t CircularAudioBuffer<SampleType>::push(const SampleType *iBuffer, uint32_t numSamples) {

        auto destBuffer = m_buffer.data();
        const auto writeIndex = m_writeCounter.load();
        const auto readIndex = m_readCounter.load();

        auto numToCopy = std::min(numSamples, remainder());
        const auto numToWrite = std::min(numToCopy, m_capacity - writeIndex);
        memcpy(&destBuffer[writeIndex], iBuffer, numToWrite * sizeof(SampleType));

        memcpy(&destBuffer[0], &iBuffer[numToWrite], (numToCopy - numToWrite) * sizeof(SampleType));

        m_writeCounter.store((writeIndex + numToCopy) % m_capacity);

        return numToCopy;
    }

    template<typename SampleType>
    uint32_t CircularAudioBuffer<SampleType>::peek(SampleType *oBuffer, uint32_t numSamples) const {
        auto srcBuffer = m_buffer.data();
        const auto readIndex = m_readCounter.load();
        const auto writeIndex = m_writeCounter.load();

        auto numToCopy = std::min(numSamples, num());
        const auto numRead = std::min(numToCopy, m_capacity - readIndex);
        memcpy(oBuffer, &srcBuffer[readIndex], numRead * sizeof(SampleType));

        memcpy(&oBuffer[numRead], &srcBuffer[0], (numToCopy - numRead) * sizeof(SampleType));

        return numToCopy;
    }

    template<typename SampleType>
    uint32_t CircularAudioBuffer<SampleType>::pop(SampleType *outBuffer, uint32_t numSamples) {
        auto numSamplesRead = peek(outBuffer, numSamples);
        m_readCounter.store((m_readCounter.load() + numSamplesRead) % m_capacity);
        return numSamplesRead;
    }

    template<typename SampleType>
    void CircularAudioBuffer<SampleType>::setNum(uint32_t numSamples, bool retainOldestSamples) {
        if(retainOldestSamples){
            m_writeCounter.store((m_readCounter.load() + numSamples) % m_capacity);
        }else {
            int64_t readCounterNum = static_cast<int32_t>(m_writeCounter.load()) - (static_cast<int32_t>(numSamples));
            if(readCounterNum < 0){
                readCounterNum = m_capacity + readCounterNum;
            }
            m_readCounter.store(readCounterNum);
        }
    }

    template<typename SampleType>
    uint32_t CircularAudioBuffer<SampleType>::num() const {
        const auto readIndex = m_readCounter.load();
        const auto writeIndex = m_writeCounter.load();
        return (writeIndex >= readIndex) ?  writeIndex - readIndex : m_capacity - readIndex + writeIndex;
    }

}

