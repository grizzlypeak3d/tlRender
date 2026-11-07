// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/SeqIO.h>

namespace tl
{
    //! Image I/O with Apple's ImageIO, on Apple platforms: HEIC and HEIF,
    //! the photos an iPhone or an iPad takes, which nothing else here
    //! reads.
    namespace imageio
    {
        //! ImageIO decoder.
        //!
        //! The picture is turned the way its orientation says, so that a
        //! photo taken upright is upright, and converted to 8-bit sRGB.
        class TL_IO_API_TYPE Decode : public IDecode
        {
        protected:
            Decode() = default;

        public:
            TL_IO_API virtual ~Decode();

            //! Create a new decoder.
            TL_IO_API static std::shared_ptr<Decode> create();

            TL_IO_API IOInfo getInfo(
                const std::string& fileName,
                const ftk::MemFile* = nullptr) override;
            TL_IO_API VideoData readVideo(
                const std::string& fileName,
                const ftk::MemFile*,
                const OTIO_NS::RationalTime&,
                const IOOptions& = IOOptions()) override;
        };

        //! ImageIO read plugin.
        class TL_IO_API_TYPE ReadPlugin : public IReadPlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            ReadPlugin() = default;

        public:
            //! Create a new plugin.
            TL_IO_API static std::shared_ptr<ReadPlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::shared_ptr<IDecode> decode(
                const IOOptions& = IOOptions()) override;

            TL_IO_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;
        };
    }
}
