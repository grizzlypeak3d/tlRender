// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>
#include <tlRender/Core/Audio.h>

namespace tl
{
    //! Resample audio data.
    class TL_CORE_API_TYPE AudioResample
    {
        FTK_NON_COPYABLE(AudioResample);

    protected:
        void _init(
            const AudioInfo& input,
            const AudioInfo& output);

        AudioResample();

    public:
        TL_CORE_API ~AudioResample();

        //! Create a new resampler.
        TL_CORE_API static std::shared_ptr<AudioResample> create(
            const AudioInfo& input,
            const AudioInfo& ouput);

        //! Get the input audio information.
        TL_CORE_API const AudioInfo& getInputInfo() const;

        //! Get the output audio information.
        TL_CORE_API const AudioInfo& getOutputInfo() const;

        //! Resample audio data.
        TL_CORE_API std::shared_ptr<Audio> process(const std::shared_ptr<Audio>&);

        //! Get the most samples processing this many input samples can
        //! produce, counting what is held over from earlier input.
        TL_CORE_API size_t getOutputSampleCountMax(size_t) const;

        //! Resample audio data into a buffer with room for
        //! getOutputSampleCountMax() samples, and return how many were
        //! written. Nothing is allocated, which is what the audio thread
        //! needs.
        TL_CORE_API size_t process(
            const uint8_t* in,
            size_t sampleCount,
            uint8_t* out,
            size_t outSampleCountMax);

        //! Flush any remaining data.
        TL_CORE_API void flush();

    private:
        FTK_PRIVATE();
    };
}
