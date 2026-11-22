// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/FFmpegPrivate.h>

#include <tlRender/IO/RequestQueuePrivate.h>

#include <ftk/Core/LogSystem.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>

} // extern "C"

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <optional>
#include <set>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace tl
{
    namespace ffmpeg
    {
        //! The decoder limit; see getDecoderMax(). The ids are the
        //! readers; the list runs from the one used least lately.
        class DecoderLimit
        {
        public:
            static DecoderLimit& get();

            size_t getMax() const;
            void setMax(size_t);
            size_t getCount() const;

            //! Count a decoder for the reader, which is about to use it,
            //! first waiting for room; ask is how an idle reader is asked
            //! to close its decoder.
            void open(const void* id, const std::function<void()>& ask);

            //! Note the reader is using its decoder, or has finished.
            void setBusy(const void* id, bool);

            //! The reader has closed its decoder.
            void close(const void* id);

        private:
            DecoderLimit();

            struct Entry
            {
                const void* id = nullptr;
                bool busy = true;
                bool asked = false;
                std::chrono::steady_clock::time_point used;
                std::function<void()> ask;
            };
            mutable std::mutex _mutex;
            std::condition_variable _cv;
            size_t _max = 0;
            std::list<Entry> _entries;
            //! How many decoders have closed, for a waiting reader to see
            //! room being made.
            size_t _closed = 0;
        };

        struct AVIOBufferData
        {
            AVIOBufferData() = default;
            AVIOBufferData(const uint8_t* p, size_t size);

            const uint8_t* p = nullptr;
            size_t size = 0;
            size_t offset = 0;
        };

        int avIOBufferRead(void* opaque, uint8_t* buf, int bufSize);
        int64_t avIOBufferSeek(void* opaque, int64_t offset, int whence);

        const size_t avIOContextBufferSize = 4096;

        struct ReadOptions
        {
            bool yuvToRGBConversion = false;
            bool hwAccel = false;
            //! Frames as textures (the "Texture" I/O option): a frame the
            //! hardware decoded stays on the GPU, as the pixel buffer it
            //! came in, where the GPU renderer draws it. Apple platforms.
            bool textures = false;
            AudioInfo audioConvertInfo;
            bool audioMerge = Options().audioMerge;
            size_t threadCount = Options().threadCount;
            size_t videoBufferSize = 4;
            OTIO_NS::RationalTime audioBufferSize = OTIO_NS::RationalTime(2.0, 1.0);
        };

        //! Parse the reader options.
        ReadOptions getReadOptions(const IOOptions&);

        //! Find the stream of the given type to read, or -1. A stream
        //! marked as the default is preferred over the first one found.
        int findStream(AVFormatContext*, AVMediaType);

        class ReadVideo
        {
        public:
            ReadVideo(
                const std::string& fileName,
                const std::vector<ftk::MemFile>& memory,
                const ReadOptions& options,
                const std::shared_ptr<ftk::LogSystem>& logSystem);

            ~ReadVideo();

            bool isValid() const;
            const ftk::ImageInfo& getInfo() const;
            const OTIO_NS::TimeRange& getTimeRange() const;
            const VideoSourceInfo& getSource() const;
            const ftk::ImageTags& getTags() const;

            void start();
            void seek(const OTIO_NS::RationalTime&);
            bool process(const OTIO_NS::RationalTime& currentTime);

            //! The number of read/decode errors encountered, and the
            //! first error. Only accessed from the owning thread.
            size_t getErrorCount() const;
            const std::string& getErrorString() const;

            bool isBufferEmpty() const;

            //! Drop the buffered frames from before a time.
            void dropBefore(const OTIO_NS::RationalTime&);

            //! The frame for a time: the first one buffered at or after
            //! it, the way a seek finds it. It stays buffered, since the
            //! next time may want it as well -- where a frame is missing,
            //! or a frame lasts longer than a tick -- and goes with the
            //! first time after it. Handing out the next frame for each
            //! time instead ran ahead of the frames' times at every gap,
            //! so that playing and seeking showed different frames for
            //! the same time and the picture drifted from the sound.
            std::shared_ptr<ftk::Image> frameFor(const OTIO_NS::RationalTime&);

        private:
            int _decode(const OTIO_NS::RationalTime& currentTime);
            void _copy(const std::shared_ptr<ftk::Image>&, AVFrame* frame);
            void _setError(int);
            int _openCodec(const AVCodec*, bool hwAccel);
            void _initHwAccel(const AVCodec*);
            bool _hwFallback(const OTIO_NS::RationalTime&);
            void _initFrame2();
            void _initSws(AVPixelFormat srcFormat);
            static AVPixelFormat _getHwFormat(AVCodecContext*, const AVPixelFormat*);
            void _log(const std::string&, ftk::LogType = ftk::LogType::Message) const;
            void _close();

            std::string _fileName;
            ReadOptions _options;
            ftk::ImageInfo _info;
            OTIO_NS::TimeRange _timeRange;
            VideoSourceInfo _source;
            ftk::ImageTags _tags;

            AVFormatContext* _avFormatContext = nullptr;
            AVIOBufferData _avIOBufferData;
            uint8_t* _avIOContextBuffer = nullptr;
            AVIOContext* _avIOContext = nullptr;
            AVRational _avSpeed = { 24, 1 };
            int _avStream = -1;
            std::map<int, AVCodecParameters*> _avCodecParameters;
            std::map<int, AVCodecContext*> _avCodecContext;
            AVFrame* _avFrame = nullptr;
            AVFrame* _avFrame2 = nullptr;
            AVPixelFormat _avInputPixelFormat = AV_PIX_FMT_NONE;
            AVPixelFormat _avOutputPixelFormat = AV_PIX_FMT_NONE;
            SwsContext* _swsContext = nullptr;
            //! The format the scaler was built for, which is the format of
            //! the frames that were arriving when it was built rather than
            //! the one the stream declares. See _copy().
            AVPixelFormat _swsInputPixelFormat = AV_PIX_FMT_NONE;
            AVBufferRef* _hwDeviceContext = nullptr;
            AVPixelFormat _hwPixelFormat = AV_PIX_FMT_NONE;
            //! The decoder in use, and the default: they differ when a
            //! hardware-capable decoder was preferred over a software
            //! default, and the default is the fallback.
            const AVCodec* _avCodec = nullptr;
            const AVCodec* _avCodecDefault = nullptr;
            AVFrame* _swFrame = nullptr;
            bool _hwAccel = false;
            bool _hwLogged = false;
            bool _textures = false;
            //! After a seek, the frames between the keyframe and the one
            //! asked for are decoded only to be thrown away: the ones no
            //! other frame refers to are skipped instead.
            bool _seekWalk = false;
            OTIO_NS::RationalTime _seekTarget;
            //! The index entry of the keyframe a seek landed on, until the
            //! first frame after it is decoded; -1 otherwise. A keyframe
            //! is found by its decode time, and in an open GOP -- a DVD's,
            //! a broadcast's -- the frames shown just before a keyframe
            //! come after it in the file and refer back past it: landing
            //! on that keyframe for one of them, the first frame out is the
            //! keyframe, later than the frame asked for, and the seek is
            //! made again from the keyframe before.
            int _seekKeyframe = -1;
            //! Whether the rate is the frames' own rather than FFmpeg's
            //! guess: film with pulldown timestamps. Its frames sit up to
            //! a fifth of a tick off the grid, and where the cadence is
            //! half a tick off it each pair of frames rounds onto one tick
            //! and leaves the next empty -- every other frame lost, all
            //! through a movie that starts its video a few fields in. The
            //! later of such a pair goes to the tick after, which in film
            //! is always the empty one; deciding it takes the time of the
            //! frame before, so the times of the packets read are kept,
            //! and a seek starts a keyframe earlier so the frame before
            //! the one asked for has been read.
            bool _measuredRate = false;
            std::set<int64_t> _packetTimes;
            OTIO_NS::RationalTime _frameTime(int64_t) const;
            // The time of the last frame the decoder gave back: where a
            // walk forward would start from.
            std::optional<OTIO_NS::RationalTime> _decodedTime;
            std::weak_ptr<ftk::LogSystem> _logSystem;
            std::list<std::pair<OTIO_NS::RationalTime, std::shared_ptr<ftk::Image> > > _buffer;
            bool _eof = false;
            size_t _errorCount = 0;
            std::string _errorString;

            //! Packets read ahead of the decoder on a thread of their own,
            //! so that the file is not read in the decoder's time: a 29 MB
            //! 8K DNxHR packet took the decoding thread five to seven
            //! milliseconds from the page cache, a fifth of the frame, and
            //! a file on a network share waits longer. The thread owns
            //! av_read_frame() on the format context; a seek pauses it and
            //! drops what it read (see _seekFormat()), and the decoder
            //! takes packets from the queue (see _readPacket()).
            struct PacketThread
            {
                std::thread thread;
                std::mutex mutex;
                std::condition_variable cv;
                std::list<AVPacket*> packets;
                //! AVERROR_EOF, or an error, once reading has stopped.
                int error = 0;
                //! A seek wants the format context.
                bool pause = false;
                //! The thread is between reads.
                bool idle = true;
                bool stop = false;
            };
            PacketThread _packets;
            void _startPacketThread();
            void _stopPacketThread();
            int _readPacket(AVPacket*);
            int _seekFormat(int64_t timestamp, int flags);
            //! Reach a frame a short way ahead by dropping the packets
            //! queued before it, without a seek: returns whether the
            //! packet for the timestamp, or one after it, is now at the
            //! front. (The thread reads on from there; nothing is reread.)
            bool _skipPackets(int64_t timestamp);
            //! Every frame a keyframe, so a packet skipped is a frame
            //! skipped and nothing more; see seek().
            bool _intraOnly = false;
        };

        class ReadAudio
        {
        public:
            ReadAudio(
                const std::string& fileName,
                const std::vector<ftk::MemFile>&,
                const ReadOptions&);

            ~ReadAudio();

            bool isValid() const;
            const AudioInfo& getInfo() const;
            const OTIO_NS::TimeRange& getTimeRange() const;
            const AudioSourceInfo& getSource() const;
            const ftk::ImageTags& getTags() const;

            void start();
            void seek(const OTIO_NS::RationalTime&);
            bool process(
                const OTIO_NS::RationalTime& currentTime,
                size_t sampleCount);

            //! The number of read/decode errors encountered, and the
            //! first error. Only accessed from the owning thread.
            size_t getErrorCount() const;
            const std::string& getErrorString() const;

            size_t getBufferSize() const;
            void bufferCopy(uint8_t*, size_t sampleCount);

        private:
            int _decode(const OTIO_NS::RationalTime& currentTime);
            void _queueFrame(size_t streamIndex, const OTIO_NS::RationalTime& currentTime);
            void _setError(int);
            void _close();

            std::string _fileName;
            ReadOptions _options;
            AudioInfo _info;
            OTIO_NS::TimeRange _timeRange;
            AudioSourceInfo _source;
            ftk::ImageTags _tags;

            AVFormatContext* _avFormatContext = nullptr;
            AVIOBufferData _avIOBufferData;
            uint8_t* _avIOContextBuffer = nullptr;
            AVIOContext* _avIOContext = nullptr;
            //! The stream read, and with it every mono stream merged into
            //! the output as a channel (see Options::audioMerge). The first
            //! is the one seeks and times go by.
            int _avStream = -1;
            std::vector<int> _avStreams;
            std::map<int, AVCodecParameters*> _avCodecParameters;
            std::map<int, AVCodecContext*> _avCodecContext;
            AVFrame* _avFrame = nullptr;
            SwrContext* _swrContext = nullptr;
            //! Decoded samples waiting for the resampler, one queue per
            //! input plane: a plane per merged stream, per channel of a
            //! planar stream, or one holding a packed stream.
            std::vector<std::vector<uint8_t> > _planes;
            size_t _planeByteCount = 0;
            bool _flushed = false;
            std::list<std::shared_ptr<Audio> > _buffer;
            //! The samples in the buffer, kept as it changes: counted
            //! from the list each time it was asked, a request for a long
            //! range -- a waveform of a whole film -- went quadratic in
            //! the hundreds of thousands of decoded packets it holds.
            size_t _bufferSampleCount = 0;
            bool _eof = false;
            size_t _errorCount = 0;
            std::string _errorString;
        };

        // Errors are recorded by the worker thread and read through
        // getError()/getErrorCount() from any thread.
        struct ErrorMutex
        {
            std::string error;
            size_t count = 0;
            std::mutex mutex;
        };

        struct VideoRead::Private
        {
            ReadOptions options;
            std::string fileName;

            std::shared_ptr<ReadVideo> readVideo;
            //! Whether the decoder limit has asked for the decoder back;
            //! see DecoderLimit.
            std::atomic<bool> decoderAsked{ false };

            IOInfo info;
            //! Set once the open has filled the information, which
            //! never changes afterwards, so getInfo() can answer
            //! immediately instead of queueing behind the request
            //! being served.
            std::atomic<bool> infoValid{ false };
            struct InfoRequest
            {
                std::promise<IOInfo> promise;
            };
            struct VideoRequest
            {
                OTIO_NS::RationalTime time;
                IOOptions options;
                std::promise<VideoData> promise;
            };
            // The info and video queues share one condition so that the
            // thread can wait for a request on either.
            RequestCondition condition;
            RequestQueue<InfoRequest, IOInfo> infoRequests{ condition };
            RequestQueue<VideoRequest, VideoData> videoRequests{ condition };

            //! See setPlayhead().
            std::mutex playheadMutex;
            std::optional<Playhead> playhead;

            std::thread thread;
            // Only accessed from the thread above.
            OTIO_NS::RationalTime currentTime;
            //! How long a request takes, smoothed, for the frames skipped
            //! ahead to while the playhead is moving; see _run().
            double requestSeconds = 0.0;

            ErrorMutex errorMutex;
        };

        struct AudioRead::Private
        {
            ReadOptions options;

            std::shared_ptr<ReadAudio> readAudio;

            IOInfo info;
            //! See VideoRead::Private::infoValid; a file without an
            //! audio track sets it too, since empty is the answer.
            std::atomic<bool> infoValid{ false };
            struct InfoRequest
            {
                std::promise<IOInfo> promise;
            };
            struct AudioRequest
            {
                OTIO_NS::TimeRange timeRange;
                IOOptions options;
                std::promise<AudioData> promise;
            };
            RequestCondition condition;
            RequestQueue<InfoRequest, IOInfo> infoRequests{ condition };
            RequestQueue<AudioRequest, AudioData> audioRequests{ condition };

            std::thread thread;
            // Only accessed from the thread above.
            OTIO_NS::RationalTime currentTime;

            ErrorMutex errorMutex;
        };
    }
}
