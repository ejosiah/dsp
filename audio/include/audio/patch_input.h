#pragma once

#include "forwards.h"
#include "type_defs.h"
#include "circular_buffer.h"
#include <vector>
#include <atomic>
#include <span>
#include "patch_output.h"
#include "audio.h"

namespace audio {

    class PatchInput {
    public:
        PatchInput() = default;

        explicit PatchInput(const PatchOutputStrongPtr& inOutput);

        PatchInput(const PatchInput& other);

        ~PatchInput();

        PatchInput& operator=(const PatchInput& other);

        int32_t pushAudio(float sample);

        int32_t pushAudio(float left, float right);

        int32_t pushAudio(const MonoView& buffer);

        int32_t pushAudio(const MonoView& left, const MonoView& right);

        int32_t pushAudio(const InterleavedView& buffer);

        void gain(float value);

        [[nodiscard]]
        bool isOutputStillActive() const;

        [[nodiscard]]
        bool isOpen() const;

        friend class PatchMixer;
        friend class PatchSplitter;

    private:
        int32_t pushAudio(const float* inBuffer, uint32_t numSamples);

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

    int32_t PatchInput::pushAudio(float left, float right) {
        auto pushed = pushAudio(&left, 1u) + pushAudio(&right, 1u);
        return pushed/2;
    }

    int32_t PatchInput::pushAudio(float sample) {
        return pushAudio(&sample, 1u);
    }

    int32_t PatchInput::pushAudio(const MonoView& buffer) {
        if(auto outPtr = m_outputHandle.lock()){
            if(outPtr->m_info.outputChannels() == 1){
                return pushAudio(buffer.data.data, buffer.size.numFrames);
            }else {
                return pushAudio(buffer, buffer);
            }
        }
        return -1;
    }

    int32_t PatchInput::pushAudio(const MonoView &left, const MonoView &right) {
        // TODO check we support stereo
        if(auto outPtr = m_outputHandle.lock()){
            auto numChannels = outPtr->m_info.outputChannels();
            auto numFrames = std::max(left.size.numFrames, right.size.numFrames);
            auto size = numChannels * numFrames;
            std::vector<real_t> data(size);
            for(auto i = 0; i < numFrames; i++){
                data[i * numChannels] = left.getSample(0, i);
                data[i * numChannels + 1] = right.getSample(0, i);
            }
            auto interleaved = audio::wrapInterLeaved(data.data(), numChannels, numFrames);

            return pushAudio(interleaved);
        }
        return -1;

    }

    int32_t PatchInput::pushAudio(const InterleavedView& buffer) {
        // TODO check buffer format matches input patch format
        auto samples = buffer.data.data;
        auto numSamples = buffer.size.numFrames * buffer.size.numChannels;
        return pushAudio(samples, numSamples)/buffer.size.numChannels;
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

    bool PatchInput::isOpen() const {
        return isOutputStillActive() && m_outputHandle.lock()->m_info.isActive();
    }
}