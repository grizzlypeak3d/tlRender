// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Core/Export.h>
#include <tlRender/Core/Util.h>

#include <ftk/Core/Range.h>
#include <ftk/Core/Util.h>

#include <limits>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace tl
{
    //! \name Audio Types
    ///@{

    //! Audio data types.
    enum class TL_CORE_API_TYPE AudioType
    {
        None,
        S8,
        S16,
        S32,
        F32,
        F64,

        Count,
        First = None
    };
    FTK_ENUM(TL_CORE_API, AudioType);

    //! Get the byte count for the given data type.
    TL_CORE_API size_t getByteCount(AudioType);

    //! Determine the integer data type for a given byte count.
    TL_CORE_API AudioType getIntAudioType(int);

    //! Determine the floating point data type for a given byte count.
    TL_CORE_API AudioType getFloatAudioType(int);

    ///@}

    //! Audio data information.
    class TL_CORE_API_TYPE AudioInfo
    {
    public:
        TL_CORE_API AudioInfo();
        TL_CORE_API AudioInfo(
            int       channelCount,
            AudioType type,
            int       sampleRate);

        std::string name         = "Default";
        int         channelCount = 0;
        AudioType   type         = AudioType::None;
        int         sampleRate   = 0;

        //! Is the audio valid?
        bool isValid() const;

        //! Get the byte count.
        size_t getByteCount() const;

        TL_CORE_API bool operator == (const AudioInfo&) const;
        TL_CORE_API bool operator != (const AudioInfo&) const;
    };

    //! Get an audio information label.
    TL_CORE_API std::string getLabel(const AudioInfo&, bool minimal = false);

    //! Audio data.
    class TL_CORE_API_TYPE Audio : public std::enable_shared_from_this<Audio>
    {
        FTK_NON_COPYABLE(Audio);

    protected:
        Audio(const AudioInfo&, size_t sampleCount);

    public:
        TL_CORE_API ~Audio();

        //! Create new audio.
        TL_CORE_API static std::shared_ptr<Audio> create(
            const AudioInfo& info,
            size_t           sampleCount);

        //! Get the audio information.
        TL_CORE_API const AudioInfo& getInfo() const;

        //! Get the audio channel count.
        TL_CORE_API int getChannelCount() const;

        //! Get the audio data type.
        TL_CORE_API AudioType getType() const;

        //! Get the audio sample rate.
        TL_CORE_API int getSampleRate() const;

        //! Get the audio sample count.
        TL_CORE_API size_t getSampleCount() const;

        //! Is the audio valid?
        TL_CORE_API bool isValid() const;

        //! Get the audio data byte count.
        TL_CORE_API size_t getByteCount() const;

        //! Get the audio data.
        TL_CORE_API uint8_t* getData();

        //! Get the audio data.
        TL_CORE_API const uint8_t* getData() const;

        //! Zero the audio data.
        TL_CORE_API void zero();

        //! Get the number of objects currenty instantiated.
        TL_CORE_API static size_t getObjectCount();

        //! Get the total number of bytes currently used.
        TL_CORE_API static size_t getTotalByteCount();

    private:
        AudioInfo _info;
        size_t    _sampleCount = 0;
        size_t    _byteCount   = 0;
        uint8_t*  _data        = nullptr;
    };

    //! \name Utility
    ///@{

    //! Combine chunks of audio. The chunks should all have the same
    //! number of channels and type.
    TL_CORE_API std::shared_ptr<Audio> combineAudio(
        const std::list<std::shared_ptr<Audio> >&);

    //! Mix audio sources.
    TL_CORE_API std::shared_ptr<Audio> mixAudio(
        const std::vector<std::shared_ptr<Audio> >&,
        float volume,
        const std::vector<bool>& channelMute = {});

    //! Reverse audio.
    TL_CORE_API std::shared_ptr<Audio> reverseAudio(const std::shared_ptr<Audio>&);

    //! Change audio speed.
    TL_CORE_API std::shared_ptr<Audio> changeAudioSpeed(const std::shared_ptr<Audio>&, double);

    //! Convert audio data.
    TL_CORE_API std::shared_ptr<Audio> convertAudio(const std::shared_ptr<Audio>&, AudioType);

    //! Get the total sample count from a list of audio data.
    TL_CORE_API size_t getSampleCount(const std::list<std::shared_ptr<Audio> >&);

    //! Move audio data.
    TL_CORE_API void moveAudio(
        std::list<std::shared_ptr<Audio> >& in,
        uint8_t* out,
        size_t sampleCount);

    ///@}
}

#include <tlRender/Core/AudioInline.h>
