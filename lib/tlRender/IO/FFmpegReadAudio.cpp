// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/FFmpegReadPrivate.h>

#include <ftk/Core/Format.h>

#include <cstdlib>

namespace tl
{
    namespace ffmpeg
    {
        ReadAudio::ReadAudio(
            const std::string& fileName,
            const std::vector<ftk::MemFile>& memory,
            const ReadOptions& options) :
            _fileName(fileName),
            _options(options)
        {
            try
            {
                if (!memory.empty())
                {
                    _avFormatContext = avformat_alloc_context();
                    if (!_avFormatContext)
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate format context: \"{0}\"").
                            arg(fileName));
                    }

                    _avIOBufferData = AVIOBufferData(memory[0].p, memory[0].size);
                    _avIOContextBuffer = static_cast<uint8_t*>(av_malloc(avIOContextBufferSize));
                    _avIOContext = avio_alloc_context(
                        _avIOContextBuffer,
                        avIOContextBufferSize,
                        0,
                        &_avIOBufferData,
                        &avIOBufferRead,
                        nullptr,
                        &avIOBufferSeek);
                    if (!_avIOContext)
                    {
                        throw std::runtime_error(
                            ftk::Format("Cannot allocate I/O context: \"{0}\"").
                            arg(fileName));
                    }

                    _avFormatContext->pb = _avIOContext;
                }

                int r = avformat_open_input(
                    &_avFormatContext,
                    !_avFormatContext ? fileName.c_str() : nullptr,
                    nullptr,
                    nullptr);
                if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(fileName));
                }

                r = avformat_find_stream_info(_avFormatContext, 0);
                if (r < 0)
                {
                    throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(fileName));
                }
                _avStream = findStream(_avFormatContext, AVMEDIA_TYPE_AUDIO);

                // The video rate is needed only to parse the timecode tag
                // into a start time below, and is read from this reader's own
                // format context: the audio does not depend on a video reader
                // existing. A file with no video has no rate to parse the
                // timecode against.
                // Negative, so that from_timecode() below rejects it and
                // leaves the start time alone.
                double videoRate = -1.0;
                const int avVideoStream = findStream(
                    _avFormatContext,
                    AVMEDIA_TYPE_VIDEO);
                if (avVideoStream != -1)
                {
                    videoRate = av_q2d(av_guess_frame_rate(
                        _avFormatContext,
                        _avFormatContext->streams[avVideoStream],
                        nullptr));
                }

                std::string timecode = getTimecodeFromDataStream(_avFormatContext);
                if (_avStream != -1)
                {
                    //av_dump_format(_avFormatContext, _avStream, fileName.c_str(), 0);

                    auto avAudioStream = _avFormatContext->streams[_avStream];
                    auto avAudioCodecParameters = avAudioStream->codecpar;
                    auto avAudioCodec = avcodec_find_decoder(avAudioCodecParameters->codec_id);
                    if (!avAudioCodec)
                    {
                        throw std::runtime_error(ftk::Format("No audio codec found: \"{0}\"").arg(fileName));
                    }
                    _avCodecParameters[_avStream] = avcodec_parameters_alloc();
                    if (!_avCodecParameters[_avStream])
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate parameters: \"{0}\"").arg(fileName));
                    }
                    r = avcodec_parameters_copy(_avCodecParameters[_avStream], avAudioCodecParameters);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(fileName));
                    }
                    _avCodecContext[_avStream] = avcodec_alloc_context3(avAudioCodec);
                    if (!_avCodecContext[_avStream])
                    {
                        throw std::runtime_error(ftk::Format("Cannot allocate context: \"{0}\"").arg(fileName));
                    }
                    r = avcodec_parameters_to_context(_avCodecContext[_avStream], _avCodecParameters[_avStream]);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(fileName));
                    }
                    _avCodecContext[_avStream]->thread_count = options.threadCount;
                    _avCodecContext[_avStream]->thread_type = FF_THREAD_FRAME;
                    r = avcodec_open2(_avCodecContext[_avStream], avAudioCodec, 0);
                    if (r < 0)
                    {
                        throw std::runtime_error(ftk::Format("{0}: \"{1}\"").arg(getErrorLabel(r)).arg(fileName));
                    }

                    const size_t fileChannelCount = _avCodecParameters[_avStream]->ch_layout.nb_channels;
                    const AudioType fileAudioType = toAudioType(static_cast<AVSampleFormat>(
                        _avCodecParameters[_avStream]->format));
                    if (AudioType::None == fileAudioType)
                    {
                        throw std::runtime_error(ftk::Format("Unsupported audio format: \"{0}\"").arg(fileName));
                    }
                    const size_t fileSampleRate = _avCodecParameters[_avStream]->sample_rate;

                    size_t channelCount = fileChannelCount;
                    AudioType audioType = fileAudioType;
                    size_t sampleRate = fileSampleRate;
                    if (options.audioConvertInfo.isValid())
                    {
                        channelCount = options.audioConvertInfo.channelCount;
                        audioType = options.audioConvertInfo.type;
                        sampleRate = options.audioConvertInfo.sampleRate;
                    }
                    _info.channelCount = channelCount;
                    _info.type = audioType;
                    _info.sampleRate = sampleRate;

                    int64_t sampleCount = 0;
                    if (avAudioStream->duration != AV_NOPTS_VALUE)
                    {
                        AVRational r;
                        r.num = 1;
                        r.den = sampleRate;
                        sampleCount = av_rescale_q(
                            avAudioStream->duration,
                            avAudioStream->time_base,
                            r);
                    }
                    else if (_avFormatContext->duration != AV_NOPTS_VALUE)
                    {
                        AVRational r;
                        r.num = 1;
                        r.den = sampleRate;
                        sampleCount = av_rescale_q(
                            _avFormatContext->duration,
                            av_get_time_base_q(),
                            r);
                    }

                    std::optional<OTIO_NS::RationalTime> timeReference;
                    ftk::ImageTags tags;
                    AVDictionaryEntry* tag = nullptr;
                    while ((tag = av_dict_get(_avFormatContext->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
                    {
                        const std::string key(tag->key);
                        const std::string value(tag->value);
                        tags[key] = value;
                        if (ftk::compare(
                            key,
                            "timecode",
                            ftk::CaseCompare::Insensitive))
                        {
                            timecode = value;
                        }
                        else if (ftk::compare(
                            key,
                            "time_reference",
                            ftk::CaseCompare::Insensitive))
                        {
                            timeReference = OTIO_NS::RationalTime(
                                static_cast<double>(
                                    std::strtoll(value.c_str(), nullptr, 10)),
                                sampleRate);
                        }
                    }

                    OTIO_NS::RationalTime startTime(0.0, sampleRate);
                    if (!timecode.empty())
                    {
                        opentime::ErrorStatus errorStatus;
                        const OTIO_NS::RationalTime time = OTIO_NS::RationalTime::from_timecode(
                            timecode,
                            videoRate,
                            &errorStatus);
                        if (!opentime::is_error(errorStatus))
                        {
                            startTime = time.rescaled_to(sampleRate).floor();
                        }
                    }
                    else if (timeReference.has_value())
                    {
                        startTime = timeReference.value();
                    }
                    _timeRange = OTIO_NS::TimeRange(
                        startTime,
                        OTIO_NS::RationalTime(sampleCount, sampleRate));

                    for (const auto& i : tags)
                    {
                        _tags[i.first] = i.second;
                    }
                    {
                        _source.codec =
                            avcodec_get_name(_avCodecContext[_avStream]->codec_id);
                        _source.type = fileAudioType;
                        _source.channelCount = fileChannelCount;
                        _source.sampleRate = fileSampleRate;
                    }
                }
            }
            catch (...)
            {
                _close();
                throw;
            }
        }

        ReadAudio::~ReadAudio()
        {
            _close();
        }

        void ReadAudio::_close()
        {
            if (_swrContext)
            {
                swr_free(&_swrContext);
            }
            if (_avFrame)
            {
                av_frame_free(&_avFrame);
            }
            for (auto i : _avCodecContext)
            {
                avcodec_free_context(&i.second);
            }
            for (auto i : _avCodecParameters)
            {
                avcodec_parameters_free(&i.second);
            }
            if (_avFormatContext)
            {
                avformat_close_input(&_avFormatContext);
            }
            if (_avIOContext)
            {
                av_freep(&_avIOContext->buffer);
                avio_context_free(&_avIOContext);
            }
        }

        bool ReadAudio::isValid() const
        {
            return _avStream != -1;
        }

        const AudioInfo& ReadAudio::getInfo() const
        {
            return _info;
        }

        const OTIO_NS::TimeRange& ReadAudio::getTimeRange() const
        {
            return _timeRange;
        }

        const AudioSourceInfo& ReadAudio::getSource() const
    {
        return _source;
    }

    const ftk::ImageTags& ReadAudio::getTags() const
        {
            return _tags;
        }

        void ReadAudio::start()
        {
            if (_avStream != -1)
            {
                _avFrame = av_frame_alloc();
                if (!_avFrame)
                {
                    throw std::runtime_error(ftk::Format("Cannot allocate frame: \"{0}\"").arg(_fileName));
                }

                AVChannelLayout channelLayout;
                av_channel_layout_default(&channelLayout, _info.channelCount);
                const auto& avCodecParameters = _avCodecParameters[_avStream];
                swr_alloc_set_opts2(
                    &_swrContext,
                    &channelLayout,
                    fromAudioType(_info.type),
                    _info.sampleRate,
                    &avCodecParameters->ch_layout,
                    static_cast<AVSampleFormat>(avCodecParameters->format),
                    avCodecParameters->sample_rate,
                    0,
                    NULL);
                av_channel_layout_uninit(&channelLayout);
                if (!_swrContext)
                {
                    throw std::runtime_error(ftk::Format("Cannot get context: \"{0}\"").arg(_fileName));
                }
                swr_init(_swrContext);
            }
        }

        void ReadAudio::seek(const OTIO_NS::RationalTime& time)
        {

            if (_avStream != -1)
            {
                avcodec_flush_buffers(_avCodecContext[_avStream]);

                AVRational r;
                r.num = 1;
                r.den = _info.sampleRate;
                const int seekError = av_seek_frame(
                    _avFormatContext,
                    _avStream,
                    av_rescale_q(
                        time.value() - _timeRange.start_time().value(),
                        r,
                        _avFormatContext->streams[_avStream]->time_base),
                    AVSEEK_FLAG_BACKWARD);
                if (seekError < 0)
                {
                    _setError(seekError);
                }
            }

            if (_swrContext)
            {
                const int drain = swr_get_out_samples(_swrContext, 0);
                std::vector<uint8_t> tmp(drain * _info.getByteCount(), 0);
                uint8_t* tmpP[] = { tmp.data() };
                swr_convert(
                    _swrContext,
                    tmpP,
                    drain,
                    nullptr,
                    0);
            }

            _buffer.clear();
            _eof = false;
        }

        size_t ReadAudio::getErrorCount() const
        {
            return _errorCount;
        }

        const std::string& ReadAudio::getErrorString() const
        {
            return _errorString;
        }

        void ReadAudio::_setError(int error)
        {
            ++_errorCount;
            if (_errorString.empty())
            {
                _errorString = getErrorLabel(error);
            }
        }

        bool ReadAudio::process(
            const OTIO_NS::RationalTime& currentTime,
            size_t sampleCount)
        {
            bool out = false;
            const size_t bufferSampleCount = getSampleCount(_buffer);
            if (_avStream != -1 && bufferSampleCount < sampleCount)
            {
                Packet packet;
                int decoding = 0;
                while (0 == decoding)
                {
                    if (!_eof)
                    {
                        decoding = av_read_frame(_avFormatContext, packet.p);
                        if (AVERROR_EOF == decoding)
                        {
                            _eof = true;
                            decoding = 0;
                        }
                        else if (decoding < 0)
                        {
                            _setError(decoding);
                            break;
                        }
                    }
                    if ((_eof && _avStream != -1) || (_avStream == packet.p->stream_index))
                    {
                        decoding = avcodec_send_packet(
                            _avCodecContext[_avStream],
                            _eof ? nullptr : packet.p);
                        if (AVERROR_EOF == decoding)
                        {
                            decoding = 0;
                        }
                        else if (decoding < 0)
                        {
                            _setError(decoding);
                            break;
                        }
                        decoding = _decode(currentTime);
                        if (AVERROR(EAGAIN) == decoding)
                        {
                            decoding = 0;
                        }
                        else if (AVERROR_EOF == decoding)
                        {
                            const size_t bufferSize = getSampleCount(_buffer);
                            const size_t bufferMax = _options.audioBufferSize.rescaled_to(_info.sampleRate).value();
                            if (bufferSize < bufferMax)
                            {
                                auto audio = Audio::create(_info, bufferMax - bufferSize);
                                audio->zero();
                                _buffer.push_back(audio);
                            }
                            break;
                        }
                        else if (decoding < 0)
                        {
                            _setError(decoding);
                            break;
                        }
                        else if (1 == decoding)
                        {
                            out = true;
                            break;
                        }
                    }
                    if (packet.p->buf)
                    {
                        av_packet_unref(packet.p);
                    }
                }
                if (packet.p->buf)
                {
                    av_packet_unref(packet.p);
                }
            }
            return out;
        }

        size_t ReadAudio::getBufferSize() const
        {
            return getSampleCount(_buffer);
        }

        void ReadAudio::bufferCopy(uint8_t* out, size_t sampleCount)
        {
            moveAudio(_buffer, out, sampleCount);
        }

        namespace
        {
            size_t getByteCount(AVSampleFormat format)
            {
                size_t out = 0;
                switch (format)
                {
                case AV_SAMPLE_FMT_U8:
                case AV_SAMPLE_FMT_U8P:
                    out = 1;
                    break;
                case AV_SAMPLE_FMT_S16:
                case AV_SAMPLE_FMT_S16P:
                    out = 2;
                    break;
                case AV_SAMPLE_FMT_S32:
                case AV_SAMPLE_FMT_FLT:
                case AV_SAMPLE_FMT_S32P:
                case AV_SAMPLE_FMT_FLTP:
                    out = 4;
                    break;
                case AV_SAMPLE_FMT_DBL:
                case AV_SAMPLE_FMT_DBLP:
                case AV_SAMPLE_FMT_S64:
                case AV_SAMPLE_FMT_S64P:
                    out = 8;
                    break;
                default: break;
                }
                return out;
            }
        }

        int ReadAudio::_decode(const OTIO_NS::RationalTime& currentTime)
        {
            int out = 0;
            while (0 == out)
            {

                out = avcodec_receive_frame(_avCodecContext[_avStream], _avFrame);
                if (out < 0)
                {
                    return out;
                }
                const int64_t timestamp = _avFrame->pts != AV_NOPTS_VALUE ? _avFrame->pts : _avFrame->pkt_dts;

                AVRational r;
                r.num = 1;
                r.den = _info.sampleRate;
                const auto time = OTIO_NS::RationalTime(
                    _timeRange.start_time().value() +
                    av_rescale_q(
                        timestamp,
                        _avFormatContext->streams[_avStream]->time_base,
                        r),
                    _info.sampleRate);

                if (time.value() + (_avFrame->nb_samples - 1) >= currentTime.value())
                {
                    const int swrOutputSamples = swr_get_out_samples(_swrContext, _avFrame->nb_samples);
                    auto swrOutputBuffer = Audio::create(_info, swrOutputSamples);

                    std::vector<const uint8_t*> swrInputBufferP;
                    if (av_sample_fmt_is_planar(static_cast<AVSampleFormat>(_avFrame->format)))
                    {
                        int64_t offset = 0;
                        if (time.value() < currentTime.value())
                        {
                            offset = (currentTime.value() - time.value()) *
                                getByteCount(static_cast<AVSampleFormat>(_avFrame->format));
                        }
                        for (int c = 0; c < _avFrame->ch_layout.nb_channels; ++c)
                        {
                            swrInputBufferP.push_back(av_frame_get_plane_buffer(_avFrame, c)->data + offset);
                        }
                    }
                    else
                    {
                        int64_t offset = 0;
                        if (time.value() < currentTime.value())
                        {
                            offset = (currentTime.value() - time.value()) *
                                _avFrame->ch_layout.nb_channels *
                                getByteCount(static_cast<AVSampleFormat>(_avFrame->format));
                        }
                        swrInputBufferP.push_back(av_frame_get_plane_buffer(_avFrame, 0)->data + offset);
                    }

                    uint8_t* swrOutputBufferP[] = { swrOutputBuffer->getData() };

                    int64_t size = _avFrame->nb_samples;
                    if (time.value() < currentTime.value())
                    {
                        size -= currentTime.value() - time.value();
                    }
                    const int swrOutputCount = swr_convert(
                        _swrContext,
                        swrOutputBufferP,
                        swrOutputSamples,
                        swrInputBufferP.data(),
                        size);

                    auto tmp = Audio::create(_info, swrOutputCount > 0 ? swrOutputCount : 0);
                    memcpy(tmp->getData(), swrOutputBuffer->getData(), tmp->getByteCount());
                    _buffer.push_back(tmp);
                    out = 1;
                    break;
                }
            }
            return out;
        }
    }
}
