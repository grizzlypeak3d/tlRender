// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/SeqIO.h>

namespace tl
{
    //! SVG image I/O.
    namespace svg
    {
        //! SVG decoder.
        //!
        //! An SVG has no resolution of its own beyond the size the document
        //! asks for, so the size to rasterize at is settled when the decoder
        //! is made rather than per frame: the information and the image have
        //! to agree, and the information is asked for without options.
        //!
        //! Options:
        //! - SVG/Width: the width to rasterize at, in pixels.
        //! - SVG/Height: the height to rasterize at, in pixels.
        //!
        //! Giving one keeps the document's aspect ratio; giving neither uses
        //! the size the document asks for.
        class TL_API_TYPE Decode : public IDecode
        {
        protected:
            Decode(const IOOptions&);

        public:
            TL_API virtual ~Decode();

            //! Create a new decoder.
            TL_API static std::shared_ptr<Decode> create(
                const IOOptions& = IOOptions());

            TL_API IOInfo getInfo(
                const std::string& fileName,
                const ftk::MemFile* = nullptr) override;
            TL_API VideoData readVideo(
                const std::string& fileName,
                const ftk::MemFile*,
                const OTIO_NS::RationalTime&,
                const IOOptions& = IOOptions()) override;

        private:
            ftk::Size2I _requestedSize;
        };

        //! SVG read plugin.
        class TL_API_TYPE ReadPlugin : public IReadPlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            ReadPlugin() = default;

        public:
            //! Create a new plugin.
            TL_API static std::shared_ptr<ReadPlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            TL_API std::shared_ptr<IDecode> decode(
                const IOOptions& = IOOptions()) override;

            TL_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;
        };
    }
}
