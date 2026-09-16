// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/ImageScale.h>

#if defined(TLRENDER_FFMPEG)
extern "C"
{
#include <libswscale/swscale.h>
}

#include <half.h>
#endif // TLRENDER_FFMPEG

namespace tl
{
    namespace
    {
#if defined(TLRENDER_FFMPEG)
        AVPixelFormat fromImageType(ftk::ImageType value)
        {
            AVPixelFormat out = AV_PIX_FMT_NONE;
            switch (value)
            {
            case ftk::ImageType::L_U8: out = AV_PIX_FMT_GRAY8; break;
            case ftk::ImageType::L_U16: out = AV_PIX_FMT_GRAY16; break;
            case ftk::ImageType::L_F16: out = AV_PIX_FMT_GRAYF16; break;
            case ftk::ImageType::L_F32: out = AV_PIX_FMT_GRAYF32; break;

            case ftk::ImageType::LA_U8: out = AV_PIX_FMT_YA8; break;
            case ftk::ImageType::LA_U16: out = AV_PIX_FMT_YA16; break;

            case ftk::ImageType::RGB_U8: out = AV_PIX_FMT_RGB24; break;
            case ftk::ImageType::RGB_U16: out = AV_PIX_FMT_RGB48; break;
            case ftk::ImageType::RGB_F16: out = AV_PIX_FMT_RGBF16; break;
            case ftk::ImageType::RGB_F32: out = AV_PIX_FMT_RGBF32; break;

            case ftk::ImageType::RGBA_U8: out = AV_PIX_FMT_RGBA; break;
            case ftk::ImageType::RGBA_U16: out = AV_PIX_FMT_RGBA64; break;
            case ftk::ImageType::RGBA_F16: out = AV_PIX_FMT_RGBAF16; break;
            case ftk::ImageType::RGBA_F32: out = AV_PIX_FMT_RGBAF32; break;

            case ftk::ImageType::YUV_420P_U8: out = AV_PIX_FMT_YUV420P; break;
            case ftk::ImageType::YUV_422P_U8: out = AV_PIX_FMT_YUV422P; break;
            case ftk::ImageType::YUV_444P_U8: out = AV_PIX_FMT_YUV444P; break;
            case ftk::ImageType::YUV_420P_U16: out = AV_PIX_FMT_YUV420P16; break;
            case ftk::ImageType::YUV_422P_U16: out = AV_PIX_FMT_YUV422P16; break;
            case ftk::ImageType::YUV_444P_U16: out = AV_PIX_FMT_YUV444P16; break;

            case ftk::ImageType::YUV_420SP_U8: out = AV_PIX_FMT_NV12; break;
            case ftk::ImageType::YUV_420SP_U16: out = AV_PIX_FMT_P016; break;

            // Planar half float. FFmpeg orders the planes green, blue, red;
            // the planes are handed over in that order below.
            case ftk::ImageType::RGB_F16_P: out = AV_PIX_FMT_GBRPF16; break;

            // RGB_U10 is packed 10:10:10:2 with the red in the high bits,
            // which is not X2RGB10; the U32 types have no FFmpeg format.
            default: break;
            }
            return out;
        }

        //! An image type swscale will take as input, for one it will not.
        //!
        //! Only RGBA_F32 needs this. Of everything mapped above it is the
        //! single type swscale has no input support for -- packed RGBA float
        //! is missing from its table, while RGB_F32 and RGBA_F16 beside it
        //! are there. Still missing upstream as of FFmpeg 9.0.1 and its
        //! master, so this is not waiting on a release.
        //!
        //! Half rather than sixteen bit integer: these are high dynamic range
        //! images, and integer would clamp everything above one before the
        //! image was scaled. Half cannot hold what is past its own range
        //! either, but that is far enough out to be beyond what a thumbnail
        //! says anything about.
        ftk::ImageType getScaleInputType(ftk::ImageType value)
        {
            return ftk::ImageType::RGBA_F32 == value ?
                ftk::ImageType::RGBA_F16 :
                value;
        }

        //! Narrow float samples to half. The images are the same size and
        //! channel count; only the depth differs.
        void toHalf(
            const std::shared_ptr<ftk::Image>& in,
            const std::shared_ptr<ftk::Image>& out)
        {
            const ftk::ImageInfo& info = in->getInfo();
            const size_t count =
                static_cast<size_t>(info.size.w) * info.size.h *
                ftk::getChannelCount(info.type);
            const float* p = reinterpret_cast<const float*>(in->getData());
            half* q = reinterpret_cast<half*>(out->getData());
            for (size_t i = 0; i < count; ++i)
            {
                q[i] = p[i];
            }
        }

        //! Fill the per plane pointers and strides for an image. The
        //! planar YUV types are stored plane after plane with no row
        //! padding; everything else is one plane with the row alignment
        //! from the layout. A vertical mirror is a negative stride from
        //! the last row.
        void getPlanes(
            const std::shared_ptr<ftk::Image>& image,
            uint8_t* planes[4],
            int strides[4])
        {
            const ftk::ImageInfo& info = image->getInfo();
            const size_t w = info.size.w;
            const size_t h = info.size.h;
            const size_t cw = (w + 1) / 2;
            const size_t ch = (h + 1) / 2;
            uint8_t* const data = image->getData();
            for (int i = 0; i < 4; ++i)
            {
                planes[i] = nullptr;
                strides[i] = 0;
            }
            size_t planeH[4] = { h, 0, 0, 0 };
            switch (info.type)
            {
            case ftk::ImageType::YUV_420P_U8:
                planes[0] = data;
                strides[0] = w;
                planes[1] = data + w * h;
                strides[1] = cw;
                planes[2] = planes[1] + cw * ch;
                strides[2] = cw;
                planeH[1] = ch;
                planeH[2] = ch;
                break;
            case ftk::ImageType::YUV_422P_U8:
                planes[0] = data;
                strides[0] = w;
                planes[1] = data + w * h;
                strides[1] = cw;
                planes[2] = planes[1] + cw * h;
                strides[2] = cw;
                planeH[1] = h;
                planeH[2] = h;
                break;
            case ftk::ImageType::YUV_444P_U8:
                planes[0] = data;
                strides[0] = w;
                planes[1] = data + w * h;
                strides[1] = w;
                planes[2] = planes[1] + w * h;
                strides[2] = w;
                planeH[1] = h;
                planeH[2] = h;
                break;
            case ftk::ImageType::YUV_420P_U16:
                planes[0] = data;
                strides[0] = w * 2;
                planes[1] = data + w * h * 2;
                strides[1] = cw * 2;
                planes[2] = planes[1] + cw * ch * 2;
                strides[2] = cw * 2;
                planeH[1] = ch;
                planeH[2] = ch;
                break;
            case ftk::ImageType::YUV_422P_U16:
                planes[0] = data;
                strides[0] = w * 2;
                planes[1] = data + w * h * 2;
                strides[1] = cw * 2;
                planes[2] = planes[1] + cw * h * 2;
                strides[2] = cw * 2;
                planeH[1] = h;
                planeH[2] = h;
                break;
            case ftk::ImageType::YUV_444P_U16:
                planes[0] = data;
                strides[0] = w * 2;
                planes[1] = data + w * h * 2;
                strides[1] = w * 2;
                planes[2] = planes[1] + w * h * 2;
                strides[2] = w * 2;
                planeH[1] = h;
                planeH[2] = h;
                break;
            case ftk::ImageType::YUV_420SP_U8:
                planes[0] = data;
                strides[0] = w;
                planes[1] = data + w * h;
                strides[1] = cw * 2;
                planeH[1] = ch;
                break;
            case ftk::ImageType::YUV_420SP_U16:
                planes[0] = data;
                strides[0] = w * 2;
                planes[1] = data + w * h * 2;
                strides[1] = cw * 2 * 2;
                planeH[1] = ch;
                break;
            case ftk::ImageType::RGB_F16_P:
            {
                // Stored red, green, blue. FFmpeg wants green, blue, red.
                const size_t plane = w * h * 2;
                planes[0] = data + plane;
                planes[1] = data + plane * 2;
                planes[2] = data;
                strides[0] = w * 2;
                strides[1] = w * 2;
                strides[2] = w * 2;
                planeH[1] = h;
                planeH[2] = h;
                break;
            }
            default:
                planes[0] = data;
                strides[0] = info.getByteCount() / h;
                break;
            }
            if (info.layout.mirror.y)
            {
                for (int i = 0; i < 4 && planes[i]; ++i)
                {
                    planes[i] += (planeH[i] - 1) * strides[i];
                    strides[i] = -strides[i];
                }
            }
        }
#endif // TLRENDER_FFMPEG
    }

    struct ImageScale::Private
    {
        ftk::ImageInfo inputInfo;
        ftk::ImageInfo outputInfo;
#if defined(TLRENDER_FFMPEG)
        SwsContext* swsContext = nullptr;
        //! What the input is converted to when swscale will not take it as
        //! it is. Invalid when the input needs no conversion.
        ftk::ImageInfo stagingInfo;
        std::shared_ptr<ftk::Image> staging;
#endif // TLRENDER_FFMPEG
    };

    void ImageScale::_init(
        const ftk::ImageInfo& inputInfo,
        const ftk::ImageInfo& outputInfo)
    {
        FTK_P();
        p.inputInfo = inputInfo;
        p.outputInfo = outputInfo;
#if defined(TLRENDER_FFMPEG)
        if (p.inputInfo.isValid() && p.outputInfo.isValid())
        {
            const ftk::ImageType inputType = getScaleInputType(p.inputInfo.type);
            if (inputType != p.inputInfo.type)
            {
                p.stagingInfo = p.inputInfo;
                p.stagingInfo.type = inputType;
            }
            p.swsContext = sws_getContext(
                p.inputInfo.size.w,
                p.inputInfo.size.h,
                fromImageType(inputType),
                p.outputInfo.size.w,
                p.outputInfo.size.h,
                fromImageType(p.outputInfo.type),
                SWS_AREA,
                nullptr,
                nullptr,
                nullptr);
            if (p.swsContext)
            {
                const auto swsColorspace = [](ftk::YUVCoefficients value)
                {
                    int out = SWS_CS_ITU709;
                    switch (value)
                    {
                    case ftk::YUVCoefficients::BT601: out = SWS_CS_ITU601; break;
                    case ftk::YUVCoefficients::BT2020: out = SWS_CS_BT2020; break;
                    default: break;
                    }
                    return out;
                };
                sws_setColorspaceDetails(
                    p.swsContext,
                    sws_getCoefficients(swsColorspace(p.inputInfo.yuvCoefficients)),
                    ftk::VideoLevels::FullRange == p.inputInfo.videoLevels ? 1 : 0,
                    sws_getCoefficients(swsColorspace(p.outputInfo.yuvCoefficients)),
                    ftk::VideoLevels::FullRange == p.outputInfo.videoLevels ? 1 : 0,
                    0,
                    1 << 16,
                    1 << 16);
            }
        }
#endif // TLRENDER_FFMPEG
    }

    ImageScale::ImageScale() :
        _p(new Private())
    {
    }

    ImageScale::~ImageScale()
    {
        FTK_P();
#if defined(TLRENDER_FFMPEG)
        if (p.swsContext)
        {
            sws_freeContext(p.swsContext);
        }
#endif // TLRENDER_FFMPEG
    }

    std::shared_ptr<ImageScale> ImageScale::create(
        const ftk::ImageInfo& inputInfo,
        const ftk::ImageInfo& outputInfo)
    {
        auto out = std::shared_ptr<ImageScale>(new ImageScale);
        out->_init(inputInfo, outputInfo);
        return out;
    }

    const ftk::ImageInfo& ImageScale::getInputInfo() const
    {
        return _p->inputInfo;
    }

    const ftk::ImageInfo& ImageScale::getOutputInfo() const
    {
        return _p->outputInfo;
    }

    bool ImageScale::isValid() const
    {
#if defined(TLRENDER_FFMPEG)
        return _p->swsContext != nullptr;
#else // TLRENDER_FFMPEG
        return false;
#endif // TLRENDER_FFMPEG
    }

    std::shared_ptr<ftk::Image> ImageScale::process(
        const std::shared_ptr<ftk::Image>& value)
    {
        FTK_P();
        std::shared_ptr<ftk::Image> out;
#if defined(TLRENDER_FFMPEG)
        if (p.swsContext && value && value->getInfo() == p.inputInfo)
        {
            std::shared_ptr<ftk::Image> input = value;
            if (p.stagingInfo.isValid())
            {
                if (!p.staging)
                {
                    p.staging = ftk::Image::create(p.stagingInfo);
                }
                toHalf(value, p.staging);
                input = p.staging;
            }
            out = ftk::Image::create(p.outputInfo);
            uint8_t* inPlanes[4];
            int inStrides[4];
            getPlanes(input, inPlanes, inStrides);
            uint8_t* outPlanes[4];
            int outStrides[4];
            getPlanes(out, outPlanes, outStrides);
            sws_scale(
                p.swsContext,
                inPlanes,
                inStrides,
                0,
                p.inputInfo.size.h,
                outPlanes,
                outStrides);
        }
#endif // TLRENDER_FFMPEG
        return out;
    }
}
