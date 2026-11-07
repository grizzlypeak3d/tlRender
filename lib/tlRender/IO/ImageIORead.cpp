// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/ImageIO.h>

#include <ftk/Core/Format.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <cstring>

namespace tl
{
    namespace imageio
    {
        namespace
        {
            //! Releases a Core Foundation object when it goes.
            template<typename T>
            class CF
            {
            public:
                explicit CF(T value = nullptr) : _value(value) {}
                ~CF() { if (_value) CFRelease(_value); }
                CF(const CF&) = delete;
                CF& operator = (const CF&) = delete;
                T get() const { return _value; }
                explicit operator bool() const { return _value != nullptr; }

            private:
                T _value = nullptr;
            };

            CGImageSourceRef createSource(
                const std::string& fileName,
                const ftk::MemFile* memory)
            {
                CGImageSourceRef out = nullptr;
                if (memory)
                {
                    // Over the memory, not a copy of it: the memory is the
                    // caller's for as long as the source is used here.
                    CF<CFDataRef> data(CFDataCreateWithBytesNoCopy(
                        nullptr, memory->p, memory->size, kCFAllocatorNull));
                    if (data)
                    {
                        out = CGImageSourceCreateWithData(data.get(), nullptr);
                    }
                }
                else
                {
                    CF<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
                        nullptr,
                        reinterpret_cast<const UInt8*>(fileName.c_str()),
                        fileName.size(),
                        false));
                    if (url)
                    {
                        out = CGImageSourceCreateWithURL(url.get(), nullptr);
                    }
                }
                if (out &&
                    (CGImageSourceGetStatus(out) != kCGImageStatusComplete ||
                     CGImageSourceGetCount(out) < 1))
                {
                    CFRelease(out);
                    out = nullptr;
                }
                if (!out)
                {
                    throw std::runtime_error(ftk::Format(
                        "Cannot read file: \"{0}\"").arg(fileName).str());
                }
                return out;
            }

            int getInt(CFDictionaryRef properties, CFStringRef key, int value)
            {
                if (auto number = static_cast<CFNumberRef>(CFDictionaryGetValue(properties, key)))
                {
                    CFNumberGetValue(number, kCFNumberIntType, &value);
                }
                return value;
            }

            //! The size the picture is shown at: turned a quarter, by the
            //! orientations from 5 to 8, its width and height swap.
            ftk::Size2I getSize(CGImageSourceRef source, const std::string& fileName)
            {
                CF<CFDictionaryRef> properties(
                    CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr));
                if (!properties)
                {
                    throw std::runtime_error(ftk::Format(
                        "Cannot get the size: \"{0}\"").arg(fileName).str());
                }
                const int w = getInt(properties.get(), kCGImagePropertyPixelWidth, 0);
                const int h = getInt(properties.get(), kCGImagePropertyPixelHeight, 0);
                const int orientation = getInt(properties.get(), kCGImagePropertyOrientation, 1);
                if (w <= 0 || h <= 0)
                {
                    throw std::runtime_error(ftk::Format(
                        "Cannot get the size: \"{0}\"").arg(fileName).str());
                }
                return orientation >= 5 && orientation <= 8 ?
                    ftk::Size2I(h, w) :
                    ftk::Size2I(w, h);
            }

            //! What a picture holds: a wider gamut than sRGB's, as the
            //! Display P3 of an iPhone's photos, and more than eight bits.
            //! The image is made without decoding it, to ask its color
            //! space.
            struct Kind
            {
                bool p3 = false;
                bool deep = false;
            };

            Kind getKind(CGImageSourceRef source)
            {
                Kind out;
                if (CF<CFDictionaryRef> properties{ CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr) })
                {
                    out.deep = getInt(properties.get(), kCGImagePropertyDepth, 8) > 8;
                }
                if (CF<CGImageRef> image{ CGImageSourceCreateImageAtIndex(source, 0, nullptr) })
                {
                    if (CGColorSpaceRef colorSpace = CGImageGetColorSpace(image.get()))
                    {
                        out.p3 = CGColorSpaceIsWideGamutRGB(colorSpace);
                    }
                }
                return out;
            }

            //! Eight or sixteen bits, in sRGB, or for a wide picture in
            //! Display P3, which it says, as a movie says its colors: the
            //! view shows them on a wide display, and a color managed one
            //! takes them through the configuration's P3.
            ftk::ImageInfo imageInfo(const ftk::Size2I& size, const Kind& kind)
            {
                ftk::ImageInfo out(size, kind.deep ? ftk::ImageType::RGBA_U16 : ftk::ImageType::RGBA_U8);
                // Core Graphics gives the top row first.
                out.layout.mirror.y = true;
                return out;
            }

            ftk::ImageTags imageTags(const Kind& kind)
            {
                ftk::ImageTags out;
                if (kind.p3)
                {
                    out["Color Primaries"] = "smpte432";
                    out["Color Transfer"] = "iec61966-2-1";
                }
                return out;
            }
        }

        Decode::~Decode()
        {}

        std::shared_ptr<Decode> Decode::create()
        {
            return std::shared_ptr<Decode>(new Decode);
        }

        IOInfo Decode::getInfo(
            const std::string& fileName,
            const ftk::MemFile* memory)
        {
            CF<CGImageSourceRef> source(createSource(fileName, memory));
            const Kind kind = getKind(source.get());
            IOInfo out;
            out.video.push_back(imageInfo(getSize(source.get(), fileName), kind));
            out.tags = imageTags(kind);
            return out;
        }

        VideoData Decode::readVideo(
            const std::string& fileName,
            const ftk::MemFile* memory,
            const OTIO_NS::RationalTime& time,
            const IOOptions&)
        {
            CF<CGImageSourceRef> source(createSource(fileName, memory));
            const ftk::Size2I size = getSize(source.get(), fileName);
            const Kind kind = getKind(source.get());

            // The whole picture, turned the way its orientation says: what
            // the thumbnail call does when asked for the full size, from
            // the picture rather than any thumbnail stored with it.
            const int maxSize = std::max(size.w, size.h);
            CF<CFNumberRef> maxSizeNumber(CFNumberCreate(nullptr, kCFNumberIntType, &maxSize));
            const void* keys[] =
            {
                kCGImageSourceCreateThumbnailFromImageAlways,
                kCGImageSourceCreateThumbnailWithTransform,
                kCGImageSourceThumbnailMaxPixelSize,
                kCGImageSourceShouldCacheImmediately
            };
            const void* values[] =
            {
                kCFBooleanTrue,
                kCFBooleanTrue,
                maxSizeNumber.get(),
                kCFBooleanTrue
            };
            CF<CFDictionaryRef> options(CFDictionaryCreate(
                nullptr,
                keys,
                values,
                sizeof(keys) / sizeof(keys[0]),
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks));
            CF<CGImageRef> cgImage(CGImageSourceCreateThumbnailAtIndex(
                source.get(), 0, options.get()));
            if (!cgImage)
            {
                throw std::runtime_error(ftk::Format(
                    "Cannot decode file: \"{0}\"").arg(fileName).str());
            }

            // Drawn into the image's own memory, converted on the way: to
            // sRGB, which is what a picture is taken to be unless it says
            // otherwise, or for a wide one to Display P3, which its tags
            // say. Extended sRGB would hold the colors too, but its values
            // past zero and one do not survive a color managed view. The
            // alpha is premultiplied, which an opaque photo does not notice.
            VideoData out;
            out.time = time;
            out.image = ftk::Image::create(imageInfo(size, kind));
            out.image->setTags(imageTags(kind));
            std::memset(out.image->getData(), 0, out.image->getByteCount());
            CF<CGColorSpaceRef> colorSpace(CGColorSpaceCreateWithName(
                kind.p3 ? kCGColorSpaceDisplayP3 : kCGColorSpaceSRGB));
            CF<CGContextRef> context(CGBitmapContextCreate(
                out.image->getData(),
                size.w,
                size.h,
                kind.deep ? 16 : 8,
                static_cast<size_t>(size.w) * (kind.deep ? 8 : 4),
                colorSpace.get(),
                static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                    (kind.deep ? kCGBitmapByteOrder16Little : 0)));
            if (!context)
            {
                throw std::runtime_error(ftk::Format(
                    "Cannot decode file: \"{0}\"").arg(fileName).str());
            }
            CGContextSetInterpolationQuality(context.get(), kCGInterpolationHigh);
            CGContextDrawImage(context.get(), CGRectMake(0, 0, size.w, size.h), cgImage.get());
            return out;
        }
    }
}
