// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Export.h>
#include <tlRender/IO/Read.h>

namespace tl
{
    //! USD image I/O.
    namespace usd
    {
        class Render;

        //! USD draw modes.
        enum class TL_IO_API_TYPE DrawMode
        {
            Points,
            Wireframe,
            WireframeOnSurface,
            ShadedFlat,
            ShadedSmooth,
            GeomOnly,
            GeomFlat,
            GeomSmooth,

            Count,
            First = Points
        };
        FTK_ENUM(TL_IO_API, DrawMode);

        //! USD options.
        struct TL_IO_API_TYPE Options
        {
            int           renderWidth     = 1920;
            float         complexity      = 1.F;
            usd::DrawMode drawMode        = usd::DrawMode::ShadedSmooth;
            bool          enableLighting  = true;
            bool          sRGB            = true;
            size_t        stageCacheCount = 10;
            size_t        diskCacheGB     = 0;

            bool operator == (const Options&) const = default;
        };

        //! Get USD options.
        TL_IO_API IOOptions getOptions(const Options&);
        
        //! USD video reader. USD has no audio.
        class TL_IO_API_TYPE VideoRead : public IVideoRead
        {
        protected:
            void _init(
                int64_t id,
                const std::shared_ptr<Render>&,
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            VideoRead();

        public:
            TL_IO_API ~VideoRead() override;

            //! Create a new reader.
            TL_IO_API static std::shared_ptr<VideoRead> create(
                int64_t id,
                const std::shared_ptr<Render>&,
                const ftk::Path&,
                const IOOptions&,
                const std::shared_ptr<ftk::LogSystem>&);

            TL_IO_API std::future<IOInfo> getInfo() override;
            TL_IO_API std::future<VideoData> readVideo(
                const OTIO_NS::RationalTime&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API void cancelRequests() override;

        private:
            FTK_PRIVATE();
        };

        //! USD read plugin.
        class TL_IO_API_TYPE ReadPlugin : public IReadPlugin
        {
        protected:
            void _init(const std::shared_ptr<ftk::LogSystem>&);
            
            ReadPlugin();

        public:
            TL_IO_API virtual ~ReadPlugin();

            //! Create a new plugin.
            TL_IO_API static std::shared_ptr<ReadPlugin> create(
                const std::shared_ptr<ftk::LogSystem>&);
            
            TL_IO_API std::shared_ptr<IVideoRead> videoRead(
                const ftk::Path&,
                const IOOptions& = IOOptions()) override;
            TL_IO_API std::shared_ptr<IVideoRead> videoRead(
                const ftk::Path&,
                const std::vector<ftk::MemFile>&,
                const IOOptions& = IOOptions()) override;

            TL_IO_API std::string getPluginInfo(
                const IOOptions& = IOOptions()) const override;
                
        private:
            FTK_PRIVATE();
        };

        //! \name Serialize
        ///@{

        TL_IO_API void to_json(nlohmann::json&, const Options&);

        TL_IO_API void from_json(const nlohmann::json&, Options&);

        ///@}
    }
}

