#pragma once

#include "patch_output.h"
#include <numeric>
#include <mutex>
#include <cassert>
#include "patch_output.h"
#include "patch_input.h"


namespace audio {

    class PatchSplitter{
    public:
        PatchSplitter() = default;

        ~PatchSplitter() = default;

        PatchOutputStrongPtr addNewPatch(uint32_t maxLatencyInSamples, float inGain);

        int32_t pushAudio(const real_t* inBuffer, int32_t InNumSamples);

        size_t num() const;

        int32_t maxNumberOfSamplesThatCanBePushed() const;

    private:
        void addPendingPatches();

    private:
        std::vector<PatchInput> m_pendingOutputs;
        mutable std::mutex m_pendingOutputCriticalSection;

        std::vector<PatchInput> m_connectedOutputs;
        mutable std::mutex m_connectedOutputCriticalSection;

    };

    void PatchSplitter::addPendingPatches() {
        std::lock_guard<std::mutex> lk{ m_pendingOutputCriticalSection };
        m_connectedOutputs.insert(m_connectedOutputs.begin(), m_pendingOutputs.begin(), m_pendingOutputs.end());
        m_pendingOutputs.clear();
    }

    PatchOutputStrongPtr PatchSplitter::addNewPatch(uint32_t maxLatencyInSamples, float inGain) {
        PatchOutputStrongPtr strongOutputPtr = std::make_shared<PatchOutput>(maxLatencyInSamples * 2, inGain);

        {
            std::lock_guard<std::mutex> lk{ m_pendingOutputCriticalSection };
            m_pendingOutputs.emplace_back(strongOutputPtr);
        }

        return strongOutputPtr;
    }

    int32_t PatchSplitter::pushAudio(const real_t *inBuffer, int32_t inNumSamples) {
        addPendingPatches();

        std::lock_guard<std::mutex> lk{ m_connectedOutputCriticalSection };
        int32_t  minimumSamplesPushed = Maxint32;

        for(int32_t index = m_connectedOutputs.size() - 1; index >= 0; index--){
            int32_t  numSamplesPushed = m_connectedOutputs[index].pushAudio(inBuffer, inNumSamples);
            if(numSamplesPushed >= 0){
                minimumSamplesPushed = std::min(minimumSamplesPushed, numSamplesPushed);
            }else {
                m_connectedOutputs.erase(m_connectedOutputs.begin() + index);
            }
        }
        if(minimumSamplesPushed == Maxint32){
            return -1;
        }
        return minimumSamplesPushed;
    }

    size_t PatchSplitter::num() const {
        std::lock_guard<std::mutex> lk{ m_connectedOutputCriticalSection };
        return m_connectedOutputs.size();
    }

    int32_t PatchSplitter::maxNumberOfSamplesThatCanBePushed() const {
        std::lock_guard<std::mutex> lk{ m_connectedOutputCriticalSection };

        uint32_t  smallestRemainder = MaxUint32;

        for(auto& input : m_connectedOutputs){
            if(auto outputHandlerPtr = input.m_outputHandle.lock()){
                smallestRemainder = std::min(smallestRemainder, outputHandlerPtr->m_buffer.remainder());
            }
        }
        if(smallestRemainder == MaxUint32){
            return -1;
        }else {
            // if we hit this check, we need to either return an int64_t
            // or use some other method to notify the caller that all
            // outputs are disconnected
            assert(smallestRemainder <= static_cast<uint32_t>(Maxint32));
            return as<int32_t>(smallestRemainder);
        }
    }

}