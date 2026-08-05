// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/UI/TimelineItem.h>

#include <tlRender/UI/ThumbnailSystem.h>

#include <ftk/UI/Label.h>
#include <ftk/UI/Spacer.h>

namespace tl
{
    namespace ui
    {
        //! Item kinds the timeline draws.
        enum class ItemType
        {
            Video,
            Audio,
            Gap
        };

        struct TimelineItem::Private
        {
            std::shared_ptr<Player> player;
            std::optional<OTIO_NS::RationalTime> currentTime;
            std::optional<OTIO_NS::TimeRange> inOutRange;
            PlayerCacheInfo cacheInfo;
            bool stopOnScrub = true;
            std::shared_ptr<ftk::Observable<bool> > scrub;
            std::shared_ptr<ftk::Observable<std::optional<OTIO_NS::RationalTime> > > timeScrub;
            std::vector<int> frameMarkers;
            ItemColors itemColors;

            //! A clip or gap.
            struct Item
            {
                ItemType type = ItemType::Gap;
                OTIO_NS::TimeRange timeRange;
                OTIO_NS::TimeRange availableRange;
                OTIO_NS::TimeRange trimmedRange;

                //! Both colors are kept so that toggling DisplayOptions
                //! clipColors is a redraw rather than a rebuild.
                ftk::Color4F defaultColor;
                std::optional<ftk::Color4F> otioColor;

                //! Set from outside the timeline, and preferred over both:
                //! the caller knows something about this item that the
                //! timeline does not.
                std::optional<ftk::Color4F> overrideColor;

                bool enabled = true;
                std::string label;
                std::string durationLabel;

                //! Media, for clips. Resolved through the timeline so the item
                //! follows the media reference key.
                ftk::Path path;
                //! The timeline the media lives in. A bundle's media are
                //! byte ranges rather than files, so a thumbnail is asked for
                //! by naming the timeline as well as the media, and nothing
                //! has to carry the byte ranges around.
                ftk::Path timelinePath;
                IOOptions ioOptions;

                //! Horizontal placement relative to the timeline origin. This
                //! depends only on the scale, so scrolling does not invalidate
                //! it.
                int x = 0;
                int w = 0;

                //! Text, measured when the style or the display options change.
                ftk::Size2I labelSize;
                ftk::Size2I durationSize;
                std::vector<std::shared_ptr<ftk::Glyph> > labelGlyphs;
                std::vector<std::shared_ptr<ftk::Glyph> > durationGlyphs;

                //! Thumbnails and waveforms. The timeline requests these for
                //! the items in view and cancels the rest, so a long timeline
                //! does not queue work it will never draw.
                std::optional<IOInfo> ioInfo;
                InfoRequest infoRequest;
                std::map<OTIO_NS::RationalTime, ThumbnailRequest> thumbnailRequests;
                std::map<OTIO_NS::RationalTime, std::shared_ptr<ftk::Image> > thumbnails;
                std::map<OTIO_NS::RationalTime, WaveformRequest> waveformRequests;
                std::map<OTIO_NS::RationalTime, std::shared_ptr<ftk::TriMesh2F> > waveforms;

                //! One thumbnail or waveform chunk, placed relative to the
                //! item's media area. The place is kept with the thumbnail
                //! rather than counted off while drawing, so that a chunk that
                //! has not arrived yet leaves a hole instead of shifting the
                //! ones after it.
                struct Media
                {
                    int x = 0;
                    int w = 0;
                    std::shared_ptr<ftk::Image> image;
                    std::shared_ptr<ftk::TriMesh2F> mesh;
                };
                std::vector<Media> media;
            };

            struct Track
            {
                int index = 0;
                TrackType type = TrackType::None;
                OTIO_NS::TimeRange timeRange;
                std::shared_ptr<ftk::Label> label;
                std::shared_ptr<ftk::Label> durationLabel;
                std::vector<Item> items;
                ftk::Size2I size;
                ftk::Box2I geom;
                int clipHeight = 0;
                bool visible = true;
            };
            std::vector<Track> tracks;
            int firstVideoTrack = -1;
            int firstAudioTrack = -1;

            //! Stamp the item colors onto the items. Done when either the
            //! colors or the tracks change, so that a color asked for before
            //! the timeline was read is not lost.
            void itemColorsUpdate();

            //! A half open range of item indices within a track. Items within a
            //! track are ordered by time, so these come from a binary search
            //! rather than a scan.
            struct Range
            {
                size_t begin = 0;
                size_t end = 0;
            };

            //! The items to draw, and the wider band of items to ask the
            //! thumbnail system for so that scrolling does not reveal empty
            //! clips. Items that fall out of the active band have their
            //! requests cancelled.
            std::vector<Range> visible;
            std::vector<Range> active;
            std::vector<Range> activePrev;
            ftk::Box2I activeRect;

            //! Zero size widgets that carry the screenshot tags the items used
            //! to carry themselves, so the documentation tool can still find an
            //! example of each kind.
            struct TagProxies
            {
                std::shared_ptr<ftk::Spacer> videoClip;
                std::shared_ptr<ftk::Spacer> audioClip;
                std::shared_ptr<ftk::Spacer> gap;
            };
            TagProxies tagProxies;

            struct SizeData
            {
                bool init = true;
                int margin = 0;
                int spacing = 0;
                int border = 0;
                int handle = 0;
                ftk::FontInfo fontInfo;
                ftk::FontMetrics fontMetrics;
                ftk::FontInfo itemFontInfo;
                ftk::FontMetrics itemFontMetrics;
                ftk::Box2I scrollArea;
                ftk::Size2I sizeHint;
            };
            SizeData size;

            //! Meshes reused between frames so that batching does not allocate
            //! every time the playhead moves.
            struct DrawData
            {
                ftk::TriMesh2F items;
                ftk::TriMesh2F mediaBackgrounds;
                ftk::TriMesh2F waveforms;
                ftk::TriMesh2F cache;
            };
            DrawData draw;

            enum class MouseMode
            {
                None,
                CurrentTime
            };
            MouseMode mouseMode = MouseMode::None;

            std::shared_ptr<ThumbnailSystem> thumbnailSystem;

            std::shared_ptr<ftk::Observer<OTIO_NS::RationalTime> > currentTimeObserver;
            std::shared_ptr<ftk::Observer<OTIO_NS::TimeRange> > inOutRangeObserver;
            std::shared_ptr<ftk::Observer<PlayerCacheInfo> > cacheInfoObserver;
            std::shared_ptr<ftk::Observer<bool> > timeUnitsObserver;

            //! Get the items of a track that cover the given horizontal span,
            //! relative to the timeline origin.
            static Range getRange(
                const std::vector<Item>&,
                int x0,
                int x1);

            //! Get the color to draw an item with.
            static ftk::Color4F getColor(
                const Item&,
                const DisplayOptions&,
                bool enabled);

            //! Get an item's place on screen.
            static ftk::Box2I getGeom(
                const Track&,
                const Item&,
                const ftk::V2I& origin);

            //! Get the part of an item inside its border, which is what is
            //! actually drawn.
            static ftk::Box2I getInsideGeom(
                const ftk::Box2I&,
                int border);

            //! Get the part of an item that thumbnails or a waveform occupy.
            ftk::Box2I getMediaGeom(
                const ftk::Box2I& insideGeom,
                const DisplayOptions&,
                int height) const;

            //! Get the width one thumbnail occupies, from the media's aspect
            //! ratio. Zero until the media information arrives.
            static int getThumbnailWidth(
                const Item&,
                const DisplayOptions&);

            //! Ask the thumbnail system for the thumbnails or the waveform
            //! chunks an item needs, and let go of the ones it no longer does.
            void requestThumbnails(
                Item&,
                const ftk::Box2I& mediaGeom,
                const ftk::Box2I& activeRect,
                const DisplayOptions&,
                const ItemData&);
            void requestWaveforms(
                Item&,
                const ftk::Box2I& mediaGeom,
                const ftk::Box2I& activeRect,
                const DisplayOptions&,
                const ItemData&);

            void cancelRequests(Item&);
            //! Take an item's request ids without cancelling them, so that a
            //! whole timeline's worth can be cancelled in one call.
            void takeRequests(Item&, std::vector<uint64_t>&);
        };
    }
}
