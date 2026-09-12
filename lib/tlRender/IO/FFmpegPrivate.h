// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/FFmpeg.h>

#include <ftk/Core/Image.h>

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

        //! Whether a stream's pixels use the full range of their values:
        //! its color description says so, or its pixel format is one of the
        //! JPEG ones, which are full range whatever the description says.
        bool isFullRange(AVColorRange, AVPixelFormat);

        //! The YUV matrix for a stream: the one its color description
        //! names; or when it names none, BT.601 for the JPEG pixel formats,
        //! which JFIF defines that way, and otherwise the one taken from
        //! the size -- BT.601 for standard definition and BT.709 above --
        //! the way players guess. The writer converts with the same answer,
        //! so a file whose container drops the description still reads back
        //! the way it was written.
        ftk::YUVCoefficients toYUVCoefficients(
            AVColorSpace,
            AVPixelFormat,
            const ftk::Size2I&);

        //! Convert to FFmpeg.
        AVColorSpace fromYUVCoefficients(ftk::YUVCoefficients);

        //! Get the software scaler's coefficients for a YUV matrix.
        int toSwsColorspace(ftk::YUVCoefficients);

        //! Get a label for a FFmpeg error code.
        std::string getErrorLabel(int);
    }
}
