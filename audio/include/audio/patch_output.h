#pragma once

#include "forwards.h"
#include "type_defs.h"
#include "circular_buffer.h"
#include <vector>
#include <atomic>
#include <memory>

namespace audio {

    class PatchOutput {
    public:
        PatchOutput() = default;

        PatchOutput(uint32_t maxCapacity, real_t inGain = 1.0f);

        int32_t popAudio(real_t *OutBuffer, uint32_t numSamples, bool useLatestAudio);

        int32_t mixInAudio(real_t *outBuffer, uint32_t numSamples, bool useLatestAudio);

        size_t getNumSamplesAvailable() const;

        bool isInputStale() const;

        static void mixInBuffer(const real_t* inBuffer, float* bufferToSumTo, uint32_t numSamples, float gain);

        friend class PatchInput;

        friend class PatchMixer;

        friend class PatchSplitter;

    private:
        CircularAudioBuffer<real_t> m_buffer{};
        std::vector<real_t> m_mixingBuffer{};
        std::atomic<real_t> m_targetGain{0.0};
        std::atomic<int32_t> m_numAliveInputs{0};
    };

    using PatchOutputStrongPtr = std::shared_ptr<PatchOutput>;
    using PatchOutputWeakPtr = std::weak_ptr<PatchOutput>;

    PatchOutput::PatchOutput(uint32_t maxCapacity, real_t inGain)
            : m_buffer(maxCapacity)
            , m_targetGain(inGain)
            , m_numAliveInputs(0)
    {}

    int32_t PatchOutput::popAudio(real_t *outBuffer, uint32_t numSamples, bool useLatestAudio) {
        if(isInputStale()){
            return -1;
        }

        if(useLatestAudio && m_buffer.num() > numSamples){
            m_buffer.setNum(numSamples);
        }

        int32_t numPopped = m_buffer.pop(outBuffer, numSamples);

        return numPopped;
    }

    int32_t PatchOutput::mixInAudio(real_t *outBuffer, uint32_t numSamples, bool useLatestAudio) {
        if(isInputStale()){
            return -1;
        }
        m_mixingBuffer.resize(numSamples);
        int32_t popResult{};

        if(useLatestAudio && m_buffer.num() > numSamples){
            m_buffer.setNum(numSamples);
            popResult = as<int32_t>(m_buffer.peek(m_mixingBuffer.data(), numSamples));
        }else{
            popResult = as<int32_t>(m_buffer.pop(m_mixingBuffer.data(), numSamples));
        }

        mixInBuffer(m_mixingBuffer.data(), outBuffer, popResult, m_targetGain);

        return popResult;
    }

    void PatchOutput::mixInBuffer(const real_t *inBuffer, float *bufferToSumTo, uint32_t numSamples, float gain) {
        for(uint32_t index = 0; index < numSamples; index++){
            bufferToSumTo[index] += inBuffer[index] * gain;
        }
    }

    size_t PatchOutput::getNumSamplesAvailable() const {
        return m_buffer.num();
    }

    bool PatchOutput::isInputStale() const {
        return m_numAliveInputs == 0;
    }

}