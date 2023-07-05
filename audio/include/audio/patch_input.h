#pragma once

#include "forwards.h"
#include "type_defs.h"
#include "circular_buffer.h"
#include <vector>
#include <atomic>
#include "patch_output.h"
#include <memory>

namespace audio {

    using PatchOutputStrongPtr = std::shared_ptr<PatchOutput>;
    using PatchOutputWeakPtr = std::weak_ptr<PatchOutput>;

    class PatchInput {
    public:
        PatchInput() = default;

        explicit PatchInput(const PatchOutputStrongPtr& inOutput);

        PatchInput(const PatchInput& other);

        ~PatchInput();

        PatchInput& operator=(const PatchInput& other);

        int32_t pushAudio(const float* inBuffer, uint32_t numSamples);

        void gain(float value);

        bool isOutputStillActive() const;

        friend class PatchMixer;
        friend class PatchSplitter;

    private:
       PatchOutputWeakPtr m_outputHandle{};
       uint32_t m_pushCallsCounter{0};
    };

    PatchInput::PatchInput(const PatchOutputStrongPtr &inOutput)
    : m_outputHandle(inOutput)
    {
        if(inOutput){
            inOutput->m_numAliveInputs++;
        }
    }

    PatchInput::PatchInput(const PatchInput &other){
        operator=(other);
    }

    PatchInput& PatchInput::operator=(const PatchInput &other) {
        if(&other == this){
            return *this;
        }
        m_outputHandle = other.m_outputHandle;
        m_pushCallsCounter = 0;
        if(auto strongPtr = m_outputHandle.lock()){
            strongPtr->m_numAliveInputs++;
        }
        return *this;
    }

    PatchInput::~PatchInput() {
        if(auto strongPtr = m_outputHandle.lock()){
            strongPtr->m_numAliveInputs--;
        }
    }

    int32_t PatchInput::pushAudio(const real_t *inBuffer, uint32_t numSamples) {
        auto output = m_outputHandle.lock();
        if(!output){
            return -1;
        }
        int32_t samplesPushed = output->m_buffer.push(inBuffer, numSamples);
        return samplesPushed;
    }

    void PatchInput::gain(float value) {
        if(auto output = m_outputHandle.lock()){
            output->m_targetGain = value;
        }
    }

    bool PatchInput::isOutputStillActive() const {
        return !m_outputHandle.expired();
    }
}