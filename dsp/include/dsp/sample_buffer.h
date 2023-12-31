#pragma once

#include <vector>
#include <type_traits>
#include <cstddef>
#include <cassert>
#include <cmath>

namespace dsp {

    template<typename SampleType>
    class SampleBufferView;

    template<typename SampleType, bool Circular = false, size_t Capacity = 0>
    class SampleBuffer {
    public:
        friend class SampleBufferView<SampleType>;

        SampleBuffer() {
            static_assert(std::is_floating_point_v<SampleType>, "SampleType should be floating point type");
            if constexpr (Circular){
                if(m_data.empty() || m_data.size() < Capacity){
                    m_data.resize(Capacity);
                }
            }
        }

        SampleBuffer(const SampleBuffer<SampleType, Circular>& other)
                :SampleBuffer(other.m_data.begin(), other.m_data.end())
        {}

        template<typename SampleIterator>
        SampleBuffer(SampleIterator first, SampleIterator last)
                :SampleBuffer()
        {
            m_data = std::vector<SampleType>(first, last);
        }

        SampleBuffer(size_t size, SampleType val)
                :SampleBuffer()
        {
            m_data = std::vector<SampleType>(size, val);
        }

        explicit SampleBuffer(size_t size)
                :SampleBuffer()
        {
            m_data = std::vector<SampleType>(size);
        }

        template<typename = std::enable_if<!Circular>>
        void add(SampleType sample){
            m_data.push_back(sample);
        }

        template<typename SampleTypeB, bool CircularB, typename = std::enable_if<std::is_same_v<SampleType, SampleTypeB>>, typename = std::enable_if<!Circular>>
        void add(const SampleBuffer<SampleTypeB, CircularB>& buffer) {
            auto offset = m_data.size();
            m_data.resize(m_data.size() + buffer.size());
            std::memcpy(m_data.data() + offset, buffer.m_data.data(), buffer.size());
        }

        auto size() const noexcept {
            return m_data.size();
        }

        const SampleType& operator[](const int idx) const {
            auto index = idx;
            if constexpr (Circular){
                const auto size = m_data.size();
                index = (size + index)%size;
            }
            return m_data[index];
        }

        SampleType& operator[](const int idx) {
            auto index = idx;
            if constexpr (Circular){
                const auto size = m_data.size();
                index = (size + index)%size;
            }
            return m_data[index];
        }

        explicit operator SampleType*() noexcept {
            return m_data.data();
        }

        explicit operator SampleType*() const noexcept {
            return m_data.data();
        }

        void clear() noexcept {
            m_data.clear();
        }

        void resize(size_t size) noexcept {
            m_data.resize(size);
        }

        auto begin() noexcept {
            return m_data.begin();
        }

        auto end() noexcept {
            return m_data.end();
        }

        auto cbegin() const noexcept {
            return m_data.cbegin();
        }

        auto cend() const noexcept {
            return m_data.cend();
        }

        SampleType* data() noexcept {
            return m_data.data();
        }

        SampleType* data() const noexcept {
            return m_data.data();
        }

        operator bool() const {
            return !m_data.empty();
        }

        SampleBufferView<SampleType> view(size_t start, size_t size);

    private:
        std::vector<SampleType> m_data;
    };

    template<typename SampleType>
    class SampleBufferView{
    public:
        SampleBufferView(SampleType* first, SampleType* last)
                : m_first(first)
                , m_last(last)
        {}

        auto size() const noexcept {
            return std::distance(m_first, m_last);
        }

        const SampleType& operator[](const int idx) const {
            return m_first[idx];
        }

        explicit operator SampleType*() const noexcept {
            return m_first;
        }

        auto cbegin() const noexcept {
            return m_first;
        }

        auto cend() const noexcept {
            return m_last;
        }

    private:
        const SampleType* m_first;
        const SampleType* m_last;
    };

    template<typename SampleType, bool Circular, size_t Capacity>
    SampleBufferView<SampleType> SampleBuffer<SampleType, Circular, Capacity>::view(size_t start, size_t size) {
        assert(start >= 0 && start < m_data.size());
        assert(size > start && size <= m_data.size());
        return SampleBuffer<SampleType, Circular>(m_data.data() + start, m_data.data() + size);
    }

    template<typename SampleType, size_t Capacity>
    using CircularBuffer = SampleBuffer<SampleType, true, Capacity>;

    template<typename SampleType, bool Circular = false>
    using Signal = SampleBuffer<SampleType, Circular>;

    template<typename SampleType, bool Circular>
    using Kernel = SampleBuffer<SampleType, Circular>;
}