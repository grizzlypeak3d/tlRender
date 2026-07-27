// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/OIIO.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/Memory.h>
#include <ftk/Core/String.h>

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imagebufalgo.h>

namespace tl
{
    namespace oiio
    {
        namespace
        {
            void oiioDiscardError()
            {
                // Retrieve and clear OIIO's pending global error. If left
                // unretrieved, OIIO emits it to stderr later; we call this on
                // the paths where we surface our own exception instead.
                OIIO::geterror();
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
                oiioDiscardError();
                std::stringstream ss;
                ss << "Cannot open file: " << fileName;
                throw std::runtime_error(ss.str());
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
                    std::stringstream ss;
                    ss << "Unsupported file: " << fileName;
                    throw std::runtime_error(ss.str());
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
                oiioDiscardError();
                std::stringstream ss;
                ss << "Cannot open file: " << fileName;
                throw std::runtime_error(ss.str());
            }

            // Find the layer.
            int layer = 0;
            const auto i = options.find("Layer");
            if (i != options.end())
            {
                layer = std::atoi(i->second.c_str());
            }
            if (!oiioInput->seek_subimage(layer, 0))
            {
                oiioDiscardError();
                std::stringstream ss;
                ss << "Cannot open layer: " << layer;
                throw std::runtime_error(ss.str());
            }

            // Get file information.
            const auto& oiioSpec = oiioInput->spec();
            const ftk::ImageType imageType = fromOIIO(oiioSpec);
            if (ftk::ImageType::None == imageType)
            {
                std::stringstream ss;
                ss << "Unsupported file: " << fileName;
                throw std::runtime_error(ss.str());
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
                oiioDiscardError();
                std::stringstream ss;
                ss << "Cannot read file: " << fileName;
                throw std::runtime_error(ss.str());
            }
            return out;
        }


    }
}
