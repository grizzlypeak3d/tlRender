// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/DetectCuts.h>

#include <tlRender/Core/ImageScale.h>

#include <algorithm>
#include <cmath>
#include <deque>

namespace tl
{
    std::vector<size_t> findCuts(
        const std::vector<CutScore>& scores,
        const DetectCutsOptions& options)
    {
        std::vector<size_t> out;
        const size_t minLength = static_cast<size_t>(std::max(options.minLength, 1));
        const size_t window = static_cast<size_t>(std::max(options.relativeWindow, 1));
        for (size_t i = 1; i < scores.size(); ++i)
        {
            const float score = scores[i].score;
            const bool afterBlack = scores[i - 1].luma <= options.blackLevel;
            const float threshold = afterBlack ?
                options.blackThreshold :
                options.threshold;
            bool cut = score >= threshold;
            if (!cut &&
                options.relativeRatio > 0.F &&
                score >= options.relativeMin)
            {
                // Against the frames around it, itself left out. The first
                // frame has no score and is left out too.
                double sum = 0.0;
                float max = 0.F;
                size_t count = 0;
                const size_t begin = std::max<size_t>(1, i > window ? i - window : 1);
                const size_t end = std::min(scores.size(), i + window + 1);
                for (size_t j = begin; j < end; ++j)
                {
                    if (j != i)
                    {
                        sum += scores[j].score;
                        max = std::max(max, scores[j].score);
                        ++count;
                    }
                }
                const double average = count > 0 ? sum / count : 0.0;
                cut =
                    score >= options.relativeRatio * average &&
                    score >= options.relativeRatio * .5F * max;
            }
            if (!cut)
            {
                continue;
            }
            if (!out.empty() && i - out.back() < minLength)
            {
                // Too close to the one before to be a cut of its own.
                continue;
            }
            out.push_back(i);
        }
        return out;
    }

    namespace
    {
        //! A frame made small and gray, for scoring.
        struct Gray
        {
            ftk::Size2I size;
            std::vector<uint8_t> pixels;
        };

        class Scorer
        {
        public:
            explicit Scorer(int width) :
                _width(std::max(width, 16))
            {}

            //! Make a frame small and gray. Nothing when its type is one the
            //! scaler cannot convert.
            bool gray(const std::shared_ptr<ftk::Image>& image, Gray& out)
            {
                if (!image || !image->isValid())
                {
                    return false;
                }
                const ftk::ImageInfo& info = image->getInfo();
                if (!_scale || _scale->getInputInfo() != info)
                {
                    const int w = std::min(_width, info.size.w);
                    const int h = std::max(1, static_cast<int>(std::lround(
                        info.size.h * w / static_cast<double>(info.size.w))));
                    ftk::ImageInfo outInfo(w, h, ftk::ImageType::L_U8);
                    _scale = ImageScale::create(info, outInfo);
                }
                if (!_scale->isValid())
                {
                    return false;
                }
                const auto small = _scale->process(image);
                if (!small)
                {
                    return false;
                }
                const ftk::ImageInfo& smallInfo = small->getInfo();
                out.size = smallInfo.size;
                out.pixels.resize(static_cast<size_t>(out.size.w) * out.size.h);
                // Rows may be padded: copied row by row.
                const size_t rowBytes = static_cast<size_t>(out.size.w);
                const size_t stride = small->getByteCount() / std::max(out.size.h, 1);
                const uint8_t* src = small->getData();
                for (int y = 0; y < out.size.h; ++y)
                {
                    std::copy(
                        src + y * stride,
                        src + y * stride + rowBytes,
                        out.pixels.data() + y * rowBytes);
                }
                return true;
            }

            //! Score a frame against the one before, as FFmpeg's select
            //! filter does: the mean absolute difference, and how much that
            //! changed from the frame before, the smaller of the two over
            //! 100. The second keeps steady motion from scoring as a cut.
            CutScore score(const Gray& frame)
            {
                CutScore out;
                const size_t count = frame.pixels.size();
                if (count > 0)
                {
                    uint64_t sum = 0;
                    for (uint8_t v : frame.pixels)
                    {
                        sum += v;
                    }
                    out.luma = sum / static_cast<float>(count) / 255.F;
                }
                if (_prev.size == frame.size && count > 0)
                {
                    uint64_t sad = 0;
                    for (size_t i = 0; i < count; ++i)
                    {
                        sad += std::abs(
                            static_cast<int>(frame.pixels[i]) -
                            static_cast<int>(_prev.pixels[i]));
                    }
                    const double mafd = sad / static_cast<double>(count);
                    const double diff = std::fabs(mafd - _prevMafd);
                    out.score = static_cast<float>(
                        std::clamp(std::min(mafd, diff) / 100.0, 0.0, 1.0));
                    _prevMafd = mafd;
                }
                _prev = frame;
                return out;
            }

        private:
            int _width = 320;
            std::shared_ptr<ImageScale> _scale;
            Gray _prev;
            double _prevMafd = 0.0;
        };
    }

    std::vector<CutScore> scoreFrames(
        const std::shared_ptr<Timeline>& timeline,
        const OTIO_NS::TimeRange& range,
        const DetectCutsOptions& options,
        const std::function<bool(size_t, size_t)>& progress)
    {
        std::vector<CutScore> out;
        if (!timeline)
        {
            return out;
        }
        const double rate = range.duration().rate();
        const OTIO_NS::RationalTime start = range.start_time().rescaled_to(rate).round();
        const size_t count = static_cast<size_t>(std::max(
            range.duration().rescaled_to(rate).round().value(), 0.0));
        out.reserve(count);

        // Pixels, not textures, and a few frames asked for ahead, so the
        // decoders always have the next one in hand: reading one frame at a
        // time waits for each.
        IOOptions ioOptions;
        ioOptions["Texture"] = "0";
        const size_t ahead = std::max<size_t>(timeline->getVideoRequestMax(), 2);
        std::deque<VideoRequest> requests;
        size_t next = 0;
        Scorer scorer(options.width);
        for (size_t i = 0; i < count; ++i)
        {
            while (next < count && requests.size() < ahead)
            {
                requests.push_back(timeline->getVideo(
                    start + OTIO_NS::RationalTime(static_cast<double>(next), rate),
                    ioOptions));
                ++next;
            }
            VideoRequest request = std::move(requests.front());
            requests.pop_front();
            const VideoFrame frame = request.future.get();

            // A frame that will not read scores nothing, and the next is
            // scored against the last one that did.
            Gray gray;
            CutScore score;
            if (!frame.layers.empty() && scorer.gray(frame.layers.front().image, gray))
            {
                score = scorer.score(gray);
            }
            out.push_back(score);

            if (progress && !progress(i + 1, count))
            {
                std::vector<uint64_t> ids;
                for (const auto& r : requests)
                {
                    ids.push_back(r.id);
                }
                timeline->cancelRequests(ids);
                break;
            }
        }
        return out;
    }

    std::vector<OTIO_NS::RationalTime> detectCuts(
        const std::shared_ptr<Timeline>& timeline,
        const OTIO_NS::TimeRange& range,
        const DetectCutsOptions& options,
        const std::function<bool(size_t, size_t)>& progress)
    {
        std::vector<OTIO_NS::RationalTime> out;
        bool stopped = false;
        const auto scores = scoreFrames(
            timeline,
            range,
            options,
            [&progress, &stopped](size_t frame, size_t count)
            {
                if (progress && !progress(frame, count))
                {
                    stopped = true;
                    return false;
                }
                return true;
            });
        if (stopped)
        {
            return out;
        }
        const double rate = range.duration().rate();
        const OTIO_NS::RationalTime start = range.start_time().rescaled_to(rate).round();
        for (size_t i : findCuts(scores, options))
        {
            out.push_back(start + OTIO_NS::RationalTime(static_cast<double>(i), rate));
        }
        return out;
    }
}
