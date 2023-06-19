#pragma once

#include <pa_ringbuffer.h>
#include <pa_util.h>
#include <cstddef>
#include <vector>

template<typename T, size_t Size>
class RingBuffer{
public:
    RingBuffer(){
        static_assert(((Size - 1) & Size) == 0, "Size must be power of 2");
        m_data = PaUtil_AllocateZeroInitializedMemory(sizeof(T) * Size);
        PaUtil_InitializeRingBuffer(&m_delegate, sizeof(T), Size, m_data);
    }

    ~RingBuffer(){
        PaUtil_FreeMemory(m_data);
    }

    bool canRead() const {
        return PaUtil_GetRingBufferReadAvailable(&m_delegate) > 0;
    }

    T next() {
        T entry;
        PaUtil_ReadRingBuffer(&m_delegate, &entry, 1);
        return entry;
    }

    void write(T value) {
        PaUtil_WriteRingBuffer(&m_delegate, &value, 1);
    }

    auto writeIndex() const {
        return m_delegate.writeIndex;
    }

    auto readIndex() const {
        return m_delegate.readIndex;
    }

    void reset() {
        PaUtil_FlushRingBuffer(&m_delegate);
    }

private:
    void* m_data = nullptr;
    PaUtilRingBuffer m_delegate{};
};
