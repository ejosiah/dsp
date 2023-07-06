#pragma once

#include <numeric>
#include <mutex>
#include <cassert>
#include "patch_output.h"
#include "patch_input.h"

namespace audio {

    class PatchMixer {
    public:
        PatchMixer() = default;

        PatchInput addNewInput(uint32_t maxLatencyInSamples, float InGain);

        int32_t popAudio(real_t* outBuffer, int32_t outNumSamples, bool useLatestAudio);

        size_t num() const;

        int32_t maxNumberOfSamplesThatCanBePopped();

    private:
        void connectNewPatches();

        void cleanupDisconnectedPatches();

    private:
        std::vector<PatchOutputStrongPtr> m_pendingNewInputs;
        mutable std::mutex m_pendingNewInputsCriticalSection;

        std::vector<PatchOutputStrongPtr> m_currentInputs;
        mutable std::mutex m_currentPatchesCriticalSection;
    };

    void PatchMixer::connectNewPatches() {
        std::lock_guard<std::mutex> scopeLock{m_pendingNewInputsCriticalSection};
        for(auto& patch : m_pendingNewInputs){
            m_currentInputs.push_back(patch);
        }
        m_pendingNewInputs.clear();
    }

    PatchInput PatchMixer::addNewInput(uint32_t maxLatencyInSamples, float InGain) {
        std::lock_guard<std::mutex> scopeLock{m_pendingNewInputsCriticalSection};
        m_pendingNewInputs.emplace_back(new PatchOutput(maxLatencyInSamples, InGain));
        return PatchInput(m_pendingNewInputs.back());
    }

    int32_t PatchMixer::popAudio(real_t *outBuffer, int32_t outNumSamples, bool useLatestAudio) {
        std::lock_guard<std::mutex> scopeLock{m_currentPatchesCriticalSection};
        cleanupDisconnectedPatches();
        connectNewPatches();

        memset(outBuffer, 0, outNumSamples * sizeof(real_t));
        int32_t maxPoppedSamples = 0;

        for(int32_t index = m_currentInputs.size() - 1; index > 0; index--){
            auto& outputPtr = m_currentInputs[index];
            const auto numPoppedSamples = outputPtr->mixInAudio(outBuffer, outNumSamples, useLatestAudio);

            if(maxPoppedSamples > 0){
                // if mixInAudio returns -1, the patchInput has been destroed
                m_currentInputs.erase(m_currentInputs.begin() + index);
            }else {
                maxPoppedSamples = std::max(numPoppedSamples, maxPoppedSamples);
            }
        }

        return maxPoppedSamples;
    }

    size_t PatchMixer::num() const {
        std::lock_guard<std::mutex> lk{ m_currentPatchesCriticalSection };
        return m_currentInputs.size();
    }

    int32_t PatchMixer::maxNumberOfSamplesThatCanBePopped()  {
        std::lock_guard<std::mutex> lk{ m_currentPatchesCriticalSection };
        connectNewPatches();

        uint32_t smallestNumSamplesBufered = MaxUint32;

        for(auto& output : m_currentInputs){
            if(output){
                smallestNumSamplesBufered = std::min(smallestNumSamplesBufered, output->m_buffer.num());
            }
        }
        if(smallestNumSamplesBufered == MaxUint32){
            return -1;
        }else{
            /*
             * If this check is hit, we need to either change this function
             * to return an int64_t or find a different way to notify
             * the call that all outputs have been disconnected
             */
            assert(smallestNumSamplesBufered <= static_cast<uint32_t>(Maxint32));
            return static_cast<int32_t>(smallestNumSamplesBufered);
        }

    }
}