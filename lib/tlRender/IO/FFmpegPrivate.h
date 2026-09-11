// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/FFmpeg.h>

#include <tlRender/Core/HDR.h>

extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace tl
{
    namespace ffmpeg
    {
        //! Software scaler flags.
        const int swsScaleFlags = SWS_FAST_BILINEAR;

        //! Swap the numerator and denominator.
        AVRational swap(AVRational);

        //! Convert to HDR data.
        void toHDRData(AVFrameSideData**, int size, HDRData&);

        //! Convert from FFmpeg.
        AudioType toAudioType(AVSampleFormat);

        //! Convert to FFmpeg.
        AVSampleFormat fromAudioType(AudioType);

        //! Get the start timecode: from the video stream, a data stream
        //! (a timecode track), or the container, in that order.
        std::string getTimecode(AVFormatContext*);

        //! RAII class for FFmpeg packets.
        class Packet
        {
        public:
            Packet();
            ~Packet();

            AVPacket* p = nullptr;
        };

        //! Get a label for a FFmpeg error code.
        std::string getErrorLabel(int);
    }
}
