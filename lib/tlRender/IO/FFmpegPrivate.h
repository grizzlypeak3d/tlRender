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
        //! \name Software Scaler Flags
        //! The reader and the writer only ever convert the pixel format,
        //! never the size: the reader hands on the frame at the size it was
        //! decoded, and an export renders at the size it writes. The fast
        //! bilinear path both used does not treat a conversion at the same
        //! size as one -- it interpolates the chroma even for 4:4:4 -- so a
        //! one pixel red line in a 4:4:4 file came back as 164, 66, 21
        //! across three pixels, once for writing it and again for reading
        //! it on the CPU (DJV #876). Measured losslessly with FFV1.
        ///@{

        //! For writing. Area averaging is exact at the same size and the
        //! right filter if a size ever does change, and accurate rounding
        //! keeps ten bit values to the level: 4:4:4 color error down from
        //! 95 levels in 255 to 7, 4:2:2 and 4:2:0 unchanged, black and white
        //! edges exact in all three. Bicubic and Lanczos were no better for
        //! 4:4:4 and slightly worse for 4:2:0.
        const int swsWriteFlags = SWS_AREA | SWS_ACCURATE_RND;

        //! For reading, when the conversion to RGB is asked for rather than
        //! left to the GPU. Nearest sampling is exact for 4:4:4 and gives
        //! 4:2:2 and 4:2:0 byte for byte what the fast bilinear path did,
        //! so it mends the one case that was wrong and changes no other.
        const int swsReadFlags = SWS_POINT;

        ///@}

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
