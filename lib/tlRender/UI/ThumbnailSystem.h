// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/IO.h>

#include <ftk/Core/FileIO.h>
#include <ftk/Core/ISystem.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/Mesh.h>
#include <ftk/Core/Observable.h>
#include <ftk/Core/Path.h>

#include <future>

namespace tl
{
    namespace ui
    {
        //! Information request.
        struct TL_API_TYPE InfoRequest
        {
            uint64_t id = 0;
            std::future<IOInfo> future;
        };

        //! Video thumbnail request.
        struct TL_API_TYPE ThumbnailRequest
        {
            uint64_t id = 0;
            int height = 0;
            std::optional<OTIO_NS::RationalTime> time;
            std::future<std::shared_ptr<ftk::Image> > future;
        };

        //! Audio waveform request.
        struct TL_API_TYPE WaveformRequest
        {
            uint64_t id = 0;
            ftk::Size2I size;
            std::optional<OTIO_NS::TimeRange> timeRange;
            std::future<std::shared_ptr<ftk::TriMesh2F> > future;
        };

        //! Thumbnails cache options.
        struct TL_API_TYPE ThumbnailCacheOptions
        {
            //! Video cache size in megabytes.
            float thumbnailMB = 16.F;

            //! Audio cache size in megabytes.
            float waveformMB = 16.F;

            TL_API bool operator == (const ThumbnailCacheOptions&) const;
            TL_API bool operator != (const ThumbnailCacheOptions&) const;
        };

        //! Thumbnail system.
        class TL_API_TYPE ThumbnailSystem : public ftk::ISystem
        {
        protected:
            ThumbnailSystem(const std::shared_ptr<ftk::Context>&);

        public:
            TL_API ~ThumbnailSystem();

            TL_API void shutdown() override;

            //! Create a new system.
            TL_API static std::shared_ptr<ThumbnailSystem> create(
                const std::shared_ptr<ftk::Context>&);

            //! Get information.
            TL_API InfoRequest getInfo(
                const ftk::Path&,
                const IOOptions& = IOOptions());

            //! Get information about media inside a timeline.
            //!
            //! A bundle's media are byte ranges rather than files, so naming
            //! the timeline as well as the media is what lets them be read.
            TL_API InfoRequest getInfo(
                const ftk::Path& timelinePath,
                const ftk::Path& mediaPath,
                const IOOptions& = IOOptions());

            //! Get a video thumbnail.
            TL_API ThumbnailRequest getThumbnail(
                const ftk::Path&,
                int height,
                const std::optional<OTIO_NS::RationalTime>& = std::nullopt,
                const IOOptions& = IOOptions());

            //! Get a video thumbnail of media inside a timeline.
            TL_API ThumbnailRequest getThumbnail(
                const ftk::Path& timelinePath,
                const ftk::Path& mediaPath,
                int height,
                const std::optional<OTIO_NS::RationalTime>& = std::nullopt,
                const IOOptions& = IOOptions());

            //! Get an audio waveform.
            TL_API WaveformRequest getWaveform(
                const ftk::Path&,
                const ftk::Size2I&,
                const std::optional<OTIO_NS::TimeRange>& = std::nullopt,
                const IOOptions& = IOOptions());

            //! Get an audio waveform of media inside a timeline.
            TL_API WaveformRequest getWaveform(
                const ftk::Path& timelinePath,
                const ftk::Path& mediaPath,
                const ftk::Size2I&,
                const std::optional<OTIO_NS::TimeRange>& = std::nullopt,
                const IOOptions& = IOOptions());

            //! Cancel pending requests.
            TL_API void cancelRequests(const std::vector<uint64_t>&);

            //! Get the cache options.
            TL_API const ThumbnailCacheOptions& getCacheOptions() const;

            //! Observe the cache opions.
            TL_API std::shared_ptr<ftk::IObservable<ThumbnailCacheOptions> > observeCacheOptions() const;

            //! Set the cache options.
            TL_API void setCacheOptions(const ThumbnailCacheOptions&);

            //! Clear the cache.
            TL_API void clearCache();

            ///@}

        private:
            void _infoRun();
            void _thumbnailRun();
            void _waveformRun();
            void _infoCancel();
            void _thumbnailCancel();
            void _waveformCancel();

            FTK_PRIVATE();
        };

        //! \name Serialize
        ///@{

        TL_API void to_json(nlohmann::json&, const ThumbnailCacheOptions&);

        TL_API void from_json(const nlohmann::json&, ThumbnailCacheOptions&);

        ///@}
    }
}
