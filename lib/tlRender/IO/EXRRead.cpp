// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/IO/EXRPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/LogSystem.h>

#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfInputPart.h>
#include <ImfMultiPartInputFile.h>

#include <array>
#include <cstring>
#include <sstream>

namespace tl
{
    namespace exr
    {
        struct IStream::Private
        {
            std::shared_ptr<ftk::FileIO> f;
            const uint8_t* p = nullptr;
            uint64_t size = 0;
            uint64_t pos = 0;
        };

        IStream::IStream(const std::string& fileName) :
            Imf::IStream(fileName.c_str()),
            _p(new Private)
        {
            FTK_P();
            p.f = ftk::FileIO::create(fileName, ftk::FileMode::Read);
            p.p = p.f->getMemP();
            p.size = p.f->getSize();
        }

        IStream::IStream(const std::string& fileName, const uint8_t* memP, size_t memSize) :
            Imf::IStream(fileName.c_str()),
            _p(new Private)
        {
            FTK_P();
            p.p = memP;
            p.size = memSize;
        }

        IStream::~IStream()
        {}

        bool IStream::isMemoryMapped() const
        {
            return _p->p;
        }

        char* IStream::readMemoryMapped(int n)
        {
            FTK_P();
            if (p.pos >= p.size || (p.pos + n) > p.size)
            {
                throw std::runtime_error(ftk::Format("Error reading file: \"{0}\"").arg(fileName()));
            }
            char* out = nullptr;
            if (p.p)
            {
                out = const_cast<char*>(reinterpret_cast<const char*>(p.p)) + p.pos;
                p.pos += n;
            }
            return out;
        }

        bool IStream::read(char c[], int n)
        {
            FTK_P();
            if (p.pos >= p.size || (p.pos + n) > p.size)
            {
                throw std::runtime_error(ftk::Format("Error reading file: \"{0}\"").arg(fileName()));
            }
            if (p.p)
            {
                std::memcpy(c, p.p + p.pos, n);
            }
            else
            {
                p.f->read(c, n);
            }
            p.pos += n;
            return p.pos < p.size;
        }

        uint64_t IStream::tellg()
        {
            return _p->pos;
        }

        void IStream::seekg(uint64_t pos)
        {
            FTK_P();
            if (p.f)
            {
                p.f->seek(pos, ftk::SeekMode::Set);
            }
            p.pos = pos;
        }

        namespace
        {
            [[maybe_unused]] std::string getLabel(Imf::PixelType value)
            {
                const std::array<std::string, 3> data =
                {
                    "UInt",
                    "Half",
                    "Float"
                };
                return data[value];
            }

            [[maybe_unused]] std::string getLabel(Imf::Compression value)
            {
                static const std::array<std::string, static_cast<size_t>(Imf::Compression::NUM_COMPRESSION_METHODS) > data =
                {
                    "None",
                    "RLE",
                    "ZIPS",
                    "ZIP",
                    "PIZ",
                    "PXR24",
                    "B44",
                    "B44A",
                    "DWAA",
                    "DWAB"
                };
                return data[value];
            }

            class File
            {
            public:
                File(
                    const std::string& fileName,
                    const ftk::MemFile* mem)
                {
                    // Open the file.
                    if (mem)
                    {
                        _s.reset(new IStream(fileName, mem->p, mem->size));
                    }
                    else
                    {
                        _s.reset(new IStream(fileName));
                    }
                    _f.reset(new Imf::MultiPartInputFile(*_s));

                    // Get the tags.
                    const int partsCount = _f->parts();
                    if (partsCount > 0)
                    {
                        readTags(_f->header(0), _info.tags);
                    }

                    // Get the layers.
                    for (int part = 0; part < partsCount; ++part)
                    {
                        std::string view;
                        const Imf::Header& imfHeader = _f->header(part);
                        if (_f->header(part).hasView())
                        {
                            view = imfHeader.view();
                            if (!view.empty() && view[0] != '.')
                            {
                                view.insert(view.begin(), '.');
                            }
                        }

                        const ftk::Box2I displayWindow = fromImath(imfHeader.displayWindow());
                        const auto& imfChannels = imfHeader.channels();
                        std::set<std::string> imfChannelNames;
                        for (auto i = imfChannels.begin(); i != imfChannels.end(); ++i)
                        {
                            imfChannelNames.insert(i.name());
                        }
                        std::set<std::string> imfDefaultChannelNames = getDefaultChannels(imfChannelNames);

                        // Add RGB and RGBA layers.
                        auto r = imfDefaultChannelNames.find("r");
                        if (r == imfDefaultChannelNames.end())
                        {
                            r = imfDefaultChannelNames.find("R");
                        }
                        auto g = imfDefaultChannelNames.find("g");
                        if (g == imfDefaultChannelNames.end())
                        {
                            g = imfDefaultChannelNames.find("G");
                        }
                        auto b = imfDefaultChannelNames.find("b");
                        if (b == imfDefaultChannelNames.end())
                        {
                            b = imfDefaultChannelNames.find("B");
                        }
                        auto a = imfDefaultChannelNames.find("a");
                        if (a == imfDefaultChannelNames.end())
                        {
                            a = imfDefaultChannelNames.find("A");
                        }
                        if (r != imfDefaultChannelNames.end() &&
                            g != imfDefaultChannelNames.end() &&
                            b != imfDefaultChannelNames.end() &&
                            a != imfDefaultChannelNames.end())
                        {
                            const Imf::PixelType imfPixelType = imfChannels[*r].type;
                            if (imfPixelType == imfChannels[*g].type &&
                                imfPixelType == imfChannels[*b].type &&
                                imfPixelType == imfChannels[*a].type &&
                                1 == imfChannels[*r].xSampling &&
                                1 == imfChannels[*r].ySampling &&
                                1 == imfChannels[*g].xSampling &&
                                1 == imfChannels[*g].ySampling &&
                                1 == imfChannels[*b].xSampling &&
                                1 == imfChannels[*b].ySampling &&
                                1 == imfChannels[*a].xSampling &&
                                1 == imfChannels[*a].ySampling)
                            {
                                ftk::ImageInfo info;
                                info.name = "RGBA" + view;
                                info.size.w = displayWindow.w();
                                info.size.h = displayWindow.h();
                                info.pixelAspectRatio = imfHeader.pixelAspectRatio();
                                info.layout.mirror.y = true;
                                switch (imfPixelType)
                                {
                                case Imf::PixelType::HALF:  info.type = ftk::ImageType::RGBA_F16; break;
                                case Imf::PixelType::FLOAT: info.type = ftk::ImageType::RGBA_F32; break;
                                case Imf::PixelType::UINT:  info.type = ftk::ImageType::RGBA_U32; break;
                                default: break;
                                }
                                if (info.type != ftk::ImageType::None)
                                {
                                    _info.video.push_back(info);
                                    Layer layer;
                                    layer.part = part;
                                    layer.channels.push_back(*r);
                                    layer.channels.push_back(*g);
                                    layer.channels.push_back(*b);
                                    layer.channels.push_back(*a);
                                    layer.pixelType = imfPixelType;
                                    _layers.push_back(layer);
                                    imfDefaultChannelNames.erase(r);
                                    imfDefaultChannelNames.erase(g);
                                    imfDefaultChannelNames.erase(b);
                                    imfDefaultChannelNames.erase(a);
                                }
                            }
                        }
                        else if (r != imfDefaultChannelNames.end() &&
                            g != imfDefaultChannelNames.end() &&
                            b != imfDefaultChannelNames.end())
                        {
                            const Imf::PixelType imfPixelType = imfChannels[*r].type;
                            if (imfPixelType == imfChannels[*g].type &&
                                imfPixelType == imfChannels[*b].type &&
                                1 == imfChannels[*r].xSampling &&
                                1 == imfChannels[*r].ySampling &&
                                1 == imfChannels[*g].xSampling &&
                                1 == imfChannels[*g].ySampling &&
                                1 == imfChannels[*b].xSampling &&
                                1 == imfChannels[*b].ySampling)
                            {
                                ftk::ImageInfo info;
                                switch (imfPixelType)
                                {
                                case Imf::PixelType::HALF:  info.type = ftk::ImageType::RGB_F16; break;
                                case Imf::PixelType::FLOAT: info.type = ftk::ImageType::RGB_F32; break;
                                case Imf::PixelType::UINT:  info.type = ftk::ImageType::RGB_U32; break;
                                default: break;
                                }
                                if (info.type != ftk::ImageType::None)
                                {
                                    info.name = "RGB" + view;
                                    info.size.w = displayWindow.w();
                                    info.size.h = displayWindow.h();
                                    info.pixelAspectRatio = imfHeader.pixelAspectRatio();
                                    info.layout.mirror.y = true;
                                    _info.video.push_back(info);
                                    Layer layer;
                                    layer.part = part;
                                    layer.channels.push_back(*r);
                                    layer.channels.push_back(*g);
                                    layer.channels.push_back(*b);
                                    layer.pixelType = imfPixelType;
                                    _layers.push_back(layer);
                                    imfDefaultChannelNames.erase(r);
                                    imfDefaultChannelNames.erase(g);
                                    imfDefaultChannelNames.erase(b);
                                }
                            }
                        }

                        // Add remaining default layers.
                        for (const auto& imfChannelName : imfDefaultChannelNames)
                        {
                            ftk::ImageInfo info;
                            const Imf::PixelType imfPixelType = imfChannels[imfChannelName].type;
                            switch (imfPixelType)
                            {
                            case Imf::PixelType::HALF:  info.type = ftk::ImageType::L_F16; break;
                            case Imf::PixelType::FLOAT: info.type = ftk::ImageType::L_F32; break;
                            case Imf::PixelType::UINT:  info.type = ftk::ImageType::L_U32; break;
                            default: break;
                            }
                            if (info.type != ftk::ImageType::None &&
                                1 == imfChannels[imfChannelName].xSampling &&
                                1 == imfChannels[imfChannelName].ySampling)
                            {
                                info.name = imfChannelName + view;
                                info.size.w = displayWindow.w();
                                info.size.h = displayWindow.h();
                                info.pixelAspectRatio = imfHeader.pixelAspectRatio();
                                info.layout.mirror.y = true;
                                _info.video.push_back(info);
                                Layer layer;
                                layer.part = part;
                                layer.channels.push_back(imfChannelName);
                                layer.pixelType = imfPixelType;
                                _layers.push_back(layer);
                            }
                        }

                        // Add OpenEXR layers.
                        std::set<std::string> imfLayerNames;
                        imfChannels.layers(imfLayerNames);
                        for (const auto& i : imfLayerNames)
                        {
                            std::vector<std::string> imfHalfNames;
                            std::vector<std::string> imfFloatNames;
                            std::vector<std::string> imfUIntNames;
                            Imf::ChannelList::ConstIterator j0;
                            Imf::ChannelList::ConstIterator j1;
                            imfChannels.channelsInLayer(i, j0, j1);
                            for (auto j = j0; j != j1; ++j)
                            {
                                if (1 == j.channel().xSampling &&
                                    1 == j.channel().ySampling)
                                {
                                    switch (j.channel().type)
                                    {
                                    case Imf::PixelType::HALF:  imfHalfNames.push_back(j.name());  break;
                                    case Imf::PixelType::FLOAT: imfFloatNames.push_back(j.name()); break;
                                    case Imf::PixelType::UINT:  imfUIntNames.push_back(j.name());  break;
                                    default: break;
                                    }
                                }
                            }
                            ftk::ImageInfo info;
                            Layer layer;
                            if (imfHalfNames.size() > 0 && imfHalfNames.size() <= 4)
                            {
                                info.type = getFloatImageType(imfHalfNames.size(), 16);
                                reorderChannels(imfHalfNames);
                                layer.channels.insert(layer.channels.end(), imfHalfNames.begin(), imfHalfNames.end());
                                layer.pixelType = Imf::PixelType::HALF;
                            }
                            else if (imfFloatNames.size() > 0 && imfFloatNames.size() <= 4)
                            {
                                info.type = getFloatImageType(imfFloatNames.size(), 32);
                                reorderChannels(imfFloatNames);
                                layer.channels.insert(layer.channels.end(), imfFloatNames.begin(), imfFloatNames.end());
                                layer.pixelType = Imf::PixelType::FLOAT;
                            }
                            else if (imfUIntNames.size() > 0 && imfUIntNames.size() <= 4)
                            {
                                info.type = getIntImageType(imfUIntNames.size(), 16);
                                reorderChannels(imfUIntNames);
                                layer.channels.insert(layer.channels.end(), imfUIntNames.begin(), imfUIntNames.end());
                                layer.pixelType = Imf::PixelType::UINT;
                            }
                            if (info.type != ftk::ImageType::None)
                            {
                                info.name = i + view;
                                info.size.w = displayWindow.w();
                                info.size.h = displayWindow.h();
                                info.pixelAspectRatio = imfHeader.pixelAspectRatio();
                                info.layout.mirror.y = true;
                                _info.video.push_back(info);
                                layer.part = part;
                                _layers.push_back(layer);
                            }
                        }
                    }
                    if (_info.video.empty())
                    {
                        throw std::runtime_error(ftk::Format("Unsupported image type: \"{0}\"").arg(fileName));
                    }
                }

                const IOInfo& getInfo() const
                {
                    return _info;
                }

                VideoData read(
                    const std::string& fileName,
                    const OTIO_NS::RationalTime& time,
                    const IOOptions& options)
                {
                    VideoData out;
                    int layer = 0;
                    const auto i = options.find("Layer");
                    if (i != options.end())
                    {
                        layer = std::min(
                            std::atoi(i->second.c_str()),
                            static_cast<int>(_info.video.size()) - 1);
                    }
                    if (layer >= 0 && layer < static_cast<int>(_info.video.size()) && layer < static_cast<int>(_layers.size()))
                    {
                        Imf::InputPart imfPart(*_f, _layers[layer].part);
                        const Imf::Header& imfHeader = _f->header(_layers[layer].part);
                        const ftk::Box2I displayWindow = fromImath(imfHeader.displayWindow());
                        const ftk::Box2I dataWindow = fromImath(imfHeader.dataWindow());
                        const ftk::Box2I intersectedWindow = ftk::intersect(displayWindow, dataWindow);
                        const bool fast = displayWindow == dataWindow;

                        const ftk::ImageInfo& imageInfo = _info.video[layer];
                        out.image = ftk::Image::create(imageInfo);
                        out.image->setTags(_info.tags);
                        const int channels = ftk::getChannelCount(imageInfo.type);
                        const int channelByteCount = ftk::getBitDepth(imageInfo.type) / 8;
                        const int cb = channels * channelByteCount;
                        const int scb = imageInfo.size.w * channels * channelByteCount;
                        if (fast)
                        {
                            Imf::FrameBuffer frameBuffer;
                            for (int c = 0; c < channels; ++c)
                            {
                                const ftk::V2I sampling(1, 1);
                                // OpenEXR addresses pixels in absolute coordinates,
                                // so the slice base must be biased by the window
                                // origin (as the non-fast path does for the data
                                // window). Without this, a non-zero display-window
                                // origin shifts the image and writes past the end
                                // of the buffer. In the fast path display == data.
                                frameBuffer.insert(
                                    _layers[layer].channels[c],
                                    Imf::Slice(
                                        _layers[layer].pixelType,
                                        reinterpret_cast<char*>(out.image->getData())
                                            - (displayWindow.min.y * scb)
                                            - (displayWindow.min.x * cb)
                                            + (c * channelByteCount),
                                        cb,
                                        scb,
                                        sampling.x,
                                        sampling.y,
                                        0.F));
                            }
                            imfPart.setFrameBuffer(frameBuffer);
                            imfPart.readPixels(displayWindow.min.y, displayWindow.max.y);
                        }
                        else
                        {
                            Imf::FrameBuffer frameBuffer;
                            std::vector<char> buf(dataWindow.w() * cb);
                            for (int c = 0; c < channels; ++c)
                            {
                                const ftk::V2I sampling(1, 1);
                                frameBuffer.insert(
                                    _layers[layer].channels[c],
                                    Imf::Slice(
                                        _layers[layer].pixelType,
                                        buf.data() - (dataWindow.min.x * cb) + (c * channelByteCount),
                                        cb,
                                        0,
                                        sampling.x,
                                        sampling.y,
                                        0.F));
                            }
                            imfPart.setFrameBuffer(frameBuffer);
                            for (int y = displayWindow.min.y; y <= displayWindow.max.y; ++y)
                            {
                                uint8_t* p = out.image->getData() + ((y - displayWindow.min.y) * scb);
                                uint8_t* end = p + scb;
                                if (y >= intersectedWindow.min.y && y <= intersectedWindow.max.y)
                                {
                                    size_t size = (intersectedWindow.min.x - displayWindow.min.x) * cb;
                                    std::memset(p, 0, size);
                                    p += size;
                                    size = intersectedWindow.w() * cb;
                                    imfPart.readPixels(y, y);
                                    std::memcpy(
                                        p,
                                        buf.data() + std::max(displayWindow.min.x - dataWindow.min.x, 0) * cb,
                                        size);
                                    p += size;
                                }
                                std::memset(p, 0, end - p);
                            }
                        }
                    }
                    return out;
                }

            private:
                std::unique_ptr<Imf::IStream> _s;
                std::unique_ptr<Imf::MultiPartInputFile> _f;
                IOInfo _info;

                struct Layer
                {
                    int part = 0;
                    std::vector<std::string> channels;
                    Imf::PixelType pixelType = Imf::PixelType::HALF;
                };
                std::vector<Layer> _layers;
            };
        }

        Decode::Decode()
        {}

        Decode::~Decode()
        {}

        std::shared_ptr<Decode> Decode::create()
        {
            return std::shared_ptr<Decode>(new Decode);
        }

        IOInfo Decode::getInfo(
            const std::string& fileName,
            const ftk::MemFile* mem)
        {
            return File(fileName, mem).getInfo();
        }

        VideoData Decode::readVideo(
            const std::string& fileName,
            const ftk::MemFile* mem,
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            return File(fileName, mem).read(fileName, time, options);
        }

        double Decode::getSpeed(const IOInfo& info, double defaultSpeed) const
        {
            double out = defaultSpeed;
            const auto i = info.tags.find("FramesPerSecond");
            if (i != info.tags.end())
            {
                // The frames per second attribute is stored as a rational
                // value: "numerator denominator".
                std::stringstream ss(i->second);
                double n = 0.0;
                double d = 0.0;
                ss >> n >> d;
                if (d != 0.0)
                {
                    out = n / d;
                }
            }
            return out;
        }

        void Read::_init(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            // Before the base class, which starts the worker thread; the
            // first thing that thread does is ask for the information.
            _decode = Decode::create();
            ISeqRead::_init(path, mem, options, logSystem);
        }

        Read::Read()
        {}

        Read::~Read()
        {
            _finish();
        }

        std::shared_ptr<Read> Read::create(
            const ftk::Path& path,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Read>(new Read);
            out->_init(path, {}, options, logSystem);
            return out;
        }

        std::shared_ptr<Read> Read::create(
            const ftk::Path& path,
            const std::vector<ftk::MemFile>& mem,
            const IOOptions& options,
            const std::shared_ptr<ftk::LogSystem>& logSystem)
        {
            auto out = std::shared_ptr<Read>(new Read);
            out->_init(path, mem, options, logSystem);
            return out;
        }

        IOInfo Read::_getInfo(
            const std::string& fileName,
            const ftk::MemFile* mem)
        {
            IOInfo out = _decode->getInfo(fileName, mem);
            const float speed = static_cast<float>(
                _decode->getSpeed(out, _defaultSpeed));
            out.videoTime = OTIO_NS::TimeRange::range_from_start_end_time_inclusive(
                OTIO_NS::RationalTime(_startFrame, speed),
                OTIO_NS::RationalTime(_endFrame, speed));
            return out;
        }

        VideoData Read::_readVideo(
            const std::string& fileName,
            const ftk::MemFile* mem,
            const OTIO_NS::RationalTime& time,
            const IOOptions& options)
        {
            return _decode->readVideo(fileName, mem, time, options);
        }
    }
}
