// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/PNG.h>

#include <ftk/Core/PNG.h>
#include <ftk/Core/Path.h>

namespace tl
{
    namespace png
    {
        namespace
        {
            //! feather-tk takes the options as its own map, which is the same
            //! type; naming the conversion keeps the two option namespaces
            //! from looking interchangeable.
            ftk::ImageIOOptions imageIOOptions(const IOOptions& options)
            {
                return ftk::ImageIOOptions(options.begin(), options.end());
            }
        }

        Decode::Decode() :
            _plugin(new ftk::png::ImagePlugin)
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
            const auto path = ftk::toFileSystem(fileName);
            auto reader = memory ?
                _plugin->read(path, *memory) :
                _plugin->read(path);
            IOInfo out;
            out.video.push_back(reader->getInfo());
            return out;
        }

        VideoData Decode::readVideo(
            const std::string& fileName,
            const ftk::MemFile* memory,
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            const auto path = ftk::toFileSystem(fileName);
            const auto imageOptions = imageIOOptions(options);
            auto reader = memory ?
                _plugin->read(path, *memory, imageOptions) :
                _plugin->read(path, imageOptions);
            VideoData out;
            out.time = time;
            out.image = reader->read();
            return out;
        }
    }
}
