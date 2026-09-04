// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/OIIO.h>

#include <ftk/Core/Format.h>

#include <filesystem>
#include <ftk/Core/Memory.h>
#include <ftk/Core/String.h>
#include <ftk/Core/Path.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>

namespace tl
{
    namespace oiio
    {
        namespace
        {
            //! Retrieve and clear OIIO's pending global error. It has to be
            //! taken either way, or OIIO emits it to stderr later; taking it
            //! into our own exception is what says whether a file is missing
            //! rather than unreadable.
            std::string oiioTakeError()
            {
                return OIIO::geterror();
            }

            //! Append OIIO's reason to a message, when it gave one.
            std::string oiioError(const std::string& message)
            {
                const std::string error = oiioTakeError();
                return error.empty() ?
                    message :
                    ftk::Format("{0}: {1}").arg(message).arg(error).str();
            }
        }

        namespace
        {
            ftk::ImageType fromOIIO(const OIIO::ImageSpec& oiio)
            {
                ftk::ImageType out = ftk::ImageType::None;
                if (1 == oiio.nchannels)
                {
                    switch (oiio.format.basetype)
                    {
                    case OIIO::TypeDesc::UINT8:  out = ftk::ImageType::L_U8;  break;
                    case OIIO::TypeDesc::UINT16: out = ftk::ImageType::L_U16; break;
                    case OIIO::TypeDesc::UINT32: out = ftk::ImageType::L_U32; break;
                    case OIIO::TypeDesc::HALF:   out = ftk::ImageType::L_F16; break;
                    case OIIO::TypeDesc::FLOAT:  out = ftk::ImageType::L_F32; break;
                    default: break;
                    }
                }
                else if (2 == oiio.nchannels)
                {
                    switch (oiio.format.basetype)
                    {
                    case OIIO::TypeDesc::UINT8:  out = ftk::ImageType::LA_U8;  break;
                    case OIIO::TypeDesc::UINT16: out = ftk::ImageType::LA_U16; break;
                    case OIIO::TypeDesc::UINT32: out = ftk::ImageType::LA_U32; break;
                    case OIIO::TypeDesc::HALF:   out = ftk::ImageType::LA_F16; break;
                    case OIIO::TypeDesc::FLOAT:  out = ftk::ImageType::LA_F32; break;
                    default: break;
                    }
                }
                else if (3 == oiio.nchannels)
                {
                    switch (oiio.format.basetype)
                    {
                    case OIIO::TypeDesc::UINT8:  out = ftk::ImageType::RGB_U8;  break;
                    case OIIO::TypeDesc::UINT16: out = ftk::ImageType::RGB_U16; break;
                    case OIIO::TypeDesc::UINT32: out = ftk::ImageType::RGB_U32; break;
                    case OIIO::TypeDesc::HALF:   out = ftk::ImageType::RGB_F16; break;
                    case OIIO::TypeDesc::FLOAT:  out = ftk::ImageType::RGB_F32; break;
                    default: break;
                    }
                }
                else if (oiio.nchannels >= 4)
                {
                    switch (oiio.format.basetype)
                    {
                    case OIIO::TypeDesc::UINT8:  out = ftk::ImageType::RGBA_U8;  break;
                    case OIIO::TypeDesc::UINT16: out = ftk::ImageType::RGBA_U16; break;
                    case OIIO::TypeDesc::UINT32: out = ftk::ImageType::RGBA_U32; break;
                    case OIIO::TypeDesc::HALF:   out = ftk::ImageType::RGBA_F16; break;
                    case OIIO::TypeDesc::FLOAT:  out = ftk::ImageType::RGBA_F32; break;
                    default: break;
                    }
                }
                return out;
            }
        }

        Decode::Decode()
        {}

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
            // Open the file.
            std::unique_ptr<OIIO::Filesystem::IOMemReader> oiioMemReader;
            if (memory)
            {
                oiioMemReader.reset(new OIIO::Filesystem::IOMemReader(memory->p, memory->size));
            }
            const auto oiioInput = OIIO::ImageInput::open(
                fileName,
                nullptr,
                oiioMemReader.get());
            if (!oiioInput)
            {
                // Only reached once the open has failed, so looking at the
                // file system here costs nothing in the ordinary case. OIIO
                // does not say why it could not open a file, which leaves a
                // file that is missing looking like one that cannot be read.
                if (!oiioMemReader &&
                    !std::filesystem::exists(ftk::toFileSystem(fileName)))
                {
                    oiioTakeError();
                    throw std::runtime_error(ftk::Format(
                        "No such file or directory: \"{0}\"").arg(fileName).str());
                }
                throw std::runtime_error(oiioError(
                    ftk::Format("Cannot open file: \"{0}\"").arg(fileName)));
            }

            // Get file information.
            IOInfo out;
            auto oiioSpec = oiioInput->spec();
            for (const auto& i : oiioSpec.extra_attribs)
            {
                out.tags[std::string(i.name())] = i.get_string();
            }
            for (int sub = 0; oiioInput->seek_subimage(sub, 0); ++sub)
            {
                oiioSpec = oiioInput->spec();
                const ftk::ImageType imageType = fromOIIO(oiioSpec);
                if (ftk::ImageType::None == imageType)
                {
                    throw std::runtime_error(
                        ftk::Format("Unsupported file: \"{0}\"").arg(fileName).str());
                }
                ftk::ImageInfo imageInfo(oiioSpec.width, oiioSpec.height, imageType);
                if (const auto param = oiioSpec.find_attribute("oiio:subimagename"))
                {
                    imageInfo.name = param->get_string();
                }
                else
                {
                    imageInfo.name = "";
                    for (int j = 0; j < oiioSpec.nchannels; ++j)
                    {
                        imageInfo.name += oiioSpec.channelnames[j];
                    }
                }
                imageInfo.layout.mirror.y = true;
                out.video.push_back(imageInfo);
            }
            return out;
        }

        VideoData Decode::readVideo(
            const std::string& fileName,
            const ftk::MemFile* memory,
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            // Open the file.
            std::unique_ptr<OIIO::Filesystem::IOMemReader> oiioMemReader;
            if (memory)
            {
                oiioMemReader.reset(new OIIO::Filesystem::IOMemReader(memory->p, memory->size));
            }
            const auto oiioInput = OIIO::ImageInput::open(
                fileName,
                nullptr,
                oiioMemReader.get());
            if (!oiioInput)
            {
                // Only reached once the open has failed, so looking at the
                // file system here costs nothing in the ordinary case. OIIO
                // does not say why it could not open a file, which leaves a
                // file that is missing looking like one that cannot be read.
                if (!oiioMemReader &&
                    !std::filesystem::exists(ftk::toFileSystem(fileName)))
                {
                    oiioTakeError();
                    throw std::runtime_error(ftk::Format(
                        "No such file or directory: \"{0}\"").arg(fileName).str());
                }
                throw std::runtime_error(oiioError(
                    ftk::Format("Cannot open file: \"{0}\"").arg(fileName)));
            }

            // Find the layer.
            int layer = 0;
            if (const auto i = options.find("Layer"); i != options.end())
            {
                layer = std::atoi(i->second.c_str());
            }
            if (!oiioInput->seek_subimage(layer, 0))
            {
                throw std::runtime_error(oiioError(
                    ftk::Format("Cannot open layer {0}: \"{1}\"").
                    arg(layer).arg(fileName)));
            }

            // Get file information.
            const auto& oiioSpec = oiioInput->spec();
            const ftk::ImageType imageType = fromOIIO(oiioSpec);
            if (ftk::ImageType::None == imageType)
            {
                throw std::runtime_error(
                    ftk::Format("Unsupported file: \"{0}\"").arg(fileName).str());
            }

            // Get the tags.
            ftk::ImageInfo imageInfo(oiioSpec.width, oiioSpec.height, imageType);
            imageInfo.layout.mirror.y = true;
            ftk::ImageTags tags;
            for (const auto& i : oiioSpec.extra_attribs)
            {
                tags[std::string(i.name())] = i.get_string();
            }

            // Read the image.
            VideoData out;
            out.time = time;
            out.image = ftk::Image::create(imageInfo);
            out.image->setTags(tags);
            if (!oiioInput->read_image(
                layer,
                0,
                0,
                ftk::getChannelCount(imageType),
                oiioSpec.format,
                out.image->getData()))
            {
                throw std::runtime_error(oiioError(
                    ftk::Format("Cannot read file: \"{0}\"").arg(fileName)));
            }
            return out;
        }

    }
}
