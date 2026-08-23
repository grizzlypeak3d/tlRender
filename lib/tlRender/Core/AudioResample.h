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

        //! Flush any remaining data.
        TL_CORE_API void flush();

    private:
        FTK_PRIVATE();
    };
}
