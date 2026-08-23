// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/SeqIO.h>

namespace tl
{
    //! OpenEXR image I/O.
    namespace exr
    {
        //! Compression types.
        enum class TL_IO_API_TYPE Compression
        {
            None,
            RLE,
            ZIPS,
            ZIP,
            PIZ,
            PXR24,
            B44,
            B44A,
            DWAA,
            DWAB,

            Count,
            First = None
        };
        FTK_ENUM(TL_IO_API, Compression);

        //! Get default channels.
        std::set<std::string> TL_IO_API getDefaultChannels(const std::set<std::string>&);

        //! Reorder channels.
        void TL_IO_API reorderChannels(std::vector<std::string>&);

        //! OpenEXR decoder.
        class TL_IO_API_TYPE Decode : public IDecode
        {
        protected:
            Decode();

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
            TL_IO_API double getSpeed(const IOInfo&, double defaultSpeed) const override;
        };

        //! OpenEXR writer.
        class TL_IO_API_TYPE Write : public ISeqWrite
        {
        protected:
            void _init(
                const ftk::Path&,
                const IOInfo&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            Write();

        public:
            TL_IO_API virtual ~Write();

            //! Create a new writer.
            TL_IO_API static std::shared_ptr<Write> create(
                const ftk::Path&,
                const IOInfo&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

        protected:
            void _writeVideo(
                const std::string& fileName,
                const OTIO_NS::RationalTime&,
                const std::shared_ptr<ftk::Image>&,
                const IOOptions&) override;

        private:
            Compression _compression = Compression::ZIP;
            float _dwaCompressionLevel = 45.F;
        };

        //! OpenEXR read plugin.
        class TL_IO_API_TYPE ReadPlugin : public IReadPlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            ReadPlugin();

        public:
            //! Create a new plugin.
            TL_IO_API static std::shared_ptr<ReadPlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::shared_ptr<IDecode> decode(
                const IOOptions& = IOOptions()) override;

            TL_IO_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;
        };

        //! OpenEXR write plugin.
        class TL_IO_API_TYPE WritePlugin : public IWritePlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            WritePlugin();

        public:
            //! Create a new write plugin.
            TL_IO_API static std::shared_ptr<WritePlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API ftk::ImageInfo getInfo(
                const ftk::ImageInfo&,
                const IOOptions& = IOOptions()) const override;
            TL_IO_API std::shared_ptr<IWrite> write(
                const ftk::Path&,
                const IOInfo&,
                const IOOptions& = IOOptions()) override;

            TL_IO_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;
        };
    }
}
