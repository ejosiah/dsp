#pragma once

#include <span>
#include "patch_mixer.h"
#include "patch_splitter.h"

namespace audio {
    class PatchMixerSplitter{
    public:
        PatchMixerSplitter() = default;

        virtual ~PatchMixerSplitter() = default;

        PatchOutputStrongPtr  addNewOutput(uint32_t maxLatencyInSamples, float inGain);

        PatchInput addNewInput(uint32_t maxLatencyInSamples, float inGain);

        void process();

    protected:
        virtual void onProcessAudio(std::span<real_t> inAudio){}

    private:
        PatchMixer m_mixer;
        PatchSplitter m_splitter;
        std::vector<real_t> m_intermediateBuffer;
    };

    PatchOutputStrongPtr PatchMixerSplitter::addNewOutput(uint32_t maxLatencyInSamples, float inGain) {
        return m_splitter.addNewPatch(maxLatencyInSamples, inGain);
    }

    PatchInput PatchMixerSplitter::addNewInput(uint32_t maxLatencyInSamples, float inGain) {
        return m_mixer.addNewInput(maxLatencyInSamples, inGain);
    }

    void PatchMixerSplitter::process() {
        int32_t numSamplesToForward =
                std::min(m_mixer.maxNumberOfSamplesThatCanBePopped()
                         , m_splitter.maxNumberOfSamplesThatCanBePushed());

        if(numSamplesToForward <= 0){
            return;
        }

        m_intermediateBuffer.clear();
        m_intermediateBuffer.insert(m_intermediateBuffer.begin(), numSamplesToForward, 0);

        int32_t popResult = m_mixer.popAudio(m_intermediateBuffer.data(), numSamplesToForward, false);
        assert(popResult == numSamplesToForward);

        onProcessAudio(std::span<real_t>(m_intermediateBuffer.data(), m_intermediateBuffer.size()));

        int32_t pushResult = m_splitter.pushAudio(m_intermediateBuffer.data(), numSamplesToForward);
        assert(pushResult == numSamplesToForward);
    }
}