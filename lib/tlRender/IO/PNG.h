// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/SeqIO.h>

namespace ftk
{
    namespace png
    {
        class ImagePlugin;
    }
}

namespace tl
{
    //! PNG image I/O.
    //!
    //! The codec is the one feather-tk carries for its own images, so this
    //! is the only image plugin built in every configuration: a build with
    //! none of the optional media libraries can still read and write a
    //! sequence.
    namespace png
    {
        //! PNG decoder.
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

        private:
            std::shared_ptr<ftk::png::ImagePlugin> _plugin;
        };

        //! PNG writer.
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
            std::shared_ptr<ftk::png::ImagePlugin> _plugin;
        };

        //! PNG read plugin.
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

        //! PNG write plugin.
        class TL_IO_API_TYPE WritePlugin : public IWritePlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);

            WritePlugin() = default;

        public:
            //! Create a new plugin.
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
