#pragma once

#include <vector>
#include <type_traits>
#include <cstddef>

namespace dsp {

    template<typename SampleType>
    class SampleBuffer {
    public:
        SampleBuffer() {
            static_assert(std::is_floating_point_v<SampleType>, "SampleType should be floating point type");
        }

        template<typename SampleIterator>
        SampleBuffer(SampleIterator first, SampleIterator last)
        :SampleBuffer()
        {
            m_data = std::vector<SampleType>(first, last);
        }

        SampleBuffer(size_t size)
        :SampleBuffer()
        {
            m_data = std::vector<SampleType>(size);
        }

        void add(SampleType sample){
            m_data.push_back(sample);
        }

        size_t size() noexcept {
            m_data.size();
        }

        const SampleType& operator[](const int idx) const {
            return m_data[idx];
        }

        SampleType& operator[](const int idx) {
            return m_data[idx];
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

    private:
        std::vector<SampleType> m_data;
    };
}