// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CoreTest/QuantizeTest.h>

#include <tlRender/Core/Quantize.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <vector>

namespace tl
{
    namespace core_tests
    {
        QuantizeTest::QuantizeTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "core_tests::QuantizeTest")
        {}

        std::shared_ptr<QuantizeTest> QuantizeTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<QuantizeTest>(new QuantizeTest(context));
        }

        void QuantizeTest::run()
        {
            _fewColors();
            _manyColors();
            _stride();
            _empty();
            _frames();
        }

        namespace
        {
            uint32_t getColor(const uint8_t* p)
            {
                return
                    0xFF000000 |
                    (static_cast<uint32_t>(p[0]) << 16) |
                    (static_cast<uint32_t>(p[1]) << 8) |
                    static_cast<uint32_t>(p[2]);
            }
        }

        void QuantizeTest::_fewColors()
        {
            // An image with no more colors than the palette holds keeps
            // them, dithered or not: flat color stays the color it was.
            const uint8_t colors[4][3] =
            {
                { 255, 0, 0 },
                { 0, 255, 0 },
                { 0, 0, 255 },
                { 200, 180, 30 }
            };
            const int w = 32;
            const int h = 16;
            std::vector<uint8_t> rgb(w * h * 3);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const uint8_t* c = colors[(x / 8 + y / 8) % 4];
                    uint8_t* p = rgb.data() + (y * w + x) * 3;
                    p[0] = c[0];
                    p[1] = c[1];
                    p[2] = c[2];
                }
            }
            for (const bool dither : { false, true })
            {
                std::vector<uint8_t> indices(w * h);
                QuantizePalette palette;
                const size_t count = quantize(
                    rgb.data(), w * 3, w, h, indices.data(), w, palette, dither);
                FTK_CHECK(4 == count);
                bool same = true;
                for (int i = 0; i < w * h; ++i)
                {
                    same &= indices[i] < count &&
                        palette[indices[i]] == getColor(rgb.data() + i * 3);
                }
                FTK_CHECK(same);
            }
        }

        void QuantizeTest::_manyColors()
        {
            // A gradient with far more colors than the palette: the palette
            // is filled, every index is within it, and the picture comes out
            // close to what went in.
            const int w = 256;
            const int h = 256;
            std::vector<uint8_t> rgb(w * h * 3);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    uint8_t* p = rgb.data() + (y * w + x) * 3;
                    p[0] = static_cast<uint8_t>(x);
                    p[1] = static_cast<uint8_t>(y);
                    p[2] = static_cast<uint8_t>((x + y) / 2);
                }
            }
            for (const bool dither : { false, true })
            {
                std::vector<uint8_t> indices(w * h);
                QuantizePalette palette;
                const size_t count = quantize(
                    rgb.data(), w * 3, w, h, indices.data(), w, palette, dither);
                FTK_CHECK(quantizeColorsMax == count);
                std::set<uint8_t> used;
                double error = 0.0;
                int errorMax = 0;
                for (int i = 0; i < w * h; ++i)
                {
                    used.insert(indices[i]);
                    const uint32_t c = palette[indices[i]];
                    const uint8_t* p = rgb.data() + i * 3;
                    const int d[3] =
                    {
                        std::abs(static_cast<int>((c >> 16) & 0xFF) - p[0]),
                        std::abs(static_cast<int>((c >> 8) & 0xFF) - p[1]),
                        std::abs(static_cast<int>(c & 0xFF) - p[2])
                    };
                    for (int j = 0; j < 3; ++j)
                    {
                        error += d[j];
                        errorMax = std::max(errorMax, d[j]);
                    }
                }
                error /= static_cast<double>(w * h * 3);
                _print(ftk::Format("Gradient, dither {0}: {1} colors used, mean error {2}, max {3}").
                    arg(dither).
                    arg(used.size()).
                    arg(error).
                    arg(errorMax));
                FTK_CHECK(used.size() > quantizeColorsMax / 2);
                FTK_CHECK(error < 8.0);
                FTK_CHECK(errorMax < 64);
            }
        }

        void QuantizeTest::_stride()
        {
            // Rows with padding, and an image stored bottom row first: the
            // same picture gives the same indices however it is laid out.
            const int w = 5;
            const int h = 4;
            const int stride = w * 3 + 7;
            std::vector<uint8_t> packed(w * h * 3);
            std::vector<uint8_t> padded(stride * h, 99);
            std::vector<uint8_t> flipped(stride * h, 99);
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const uint8_t c[3] =
                    {
                        static_cast<uint8_t>(x * 50),
                        static_cast<uint8_t>(y * 60),
                        static_cast<uint8_t>(x * y * 10)
                    };
                    for (int j = 0; j < 3; ++j)
                    {
                        packed[(y * w + x) * 3 + j] = c[j];
                        padded[y * stride + x * 3 + j] = c[j];
                        flipped[(h - 1 - y) * stride + x * 3 + j] = c[j];
                    }
                }
            }
            QuantizePalette palette;
            std::vector<uint8_t> a(w * h);
            const size_t countA = quantize(packed.data(), w * 3, w, h, a.data(), w, palette);
            QuantizePalette paletteB;
            std::vector<uint8_t> b(w * h);
            const size_t countB = quantize(padded.data(), stride, w, h, b.data(), w, paletteB);
            QuantizePalette paletteC;
            const int indicesStride = w + 3;
            std::vector<uint8_t> c(indicesStride * h, 255);
            const size_t countC = quantize(
                flipped.data() + (h - 1) * stride, -stride, w, h, c.data(), indicesStride, paletteC);
            FTK_CHECK(countA == countB);
            FTK_CHECK(countA == countC);
            FTK_CHECK(palette == paletteB);
            FTK_CHECK(palette == paletteC);
            FTK_CHECK(a == b);
            bool same = true;
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    same &= a[y * w + x] == c[y * indicesStride + x];
                }
                // The padding is left alone.
                same &= 255 == c[y * indicesStride + w];
            }
            FTK_CHECK(same);
        }

        void QuantizeTest::_frames()
        {
            // A moving picture. A frame that differs from the last by noise
            // alone is given the same palette, and so most of its pixels the
            // same color -- the noise moves the rest, in a gradient this
            // fine, from one color to the next. Chosen afresh, the palette
            // moved with every frame, the steps of the gradient with it, and
            // a still sky wobbled.
            // The palette's last entry is transparent and no pixel is given
            // it.
            const int w = 96;
            const int h = 64;
            const auto fill = [](std::vector<uint8_t>& rgb, int noise, int shift)
            {
                unsigned int seed = 1 + noise * 7919;
                rgb.resize(w * h * 3);
                for (int y = 0; y < h; ++y)
                {
                    for (int x = 0; x < w; ++x)
                    {
                        uint8_t* p = rgb.data() + (y * w + x) * 3;
                        int n[3] = { 0, 0, 0 };
                        for (int c = 0; c < 3; ++c)
                        {
                            seed = seed * 1664525u + 1013904223u;
                            n[c] = noise ? static_cast<int>((seed >> 24) % 5) - 2 : 0;
                        }
                        p[0] = static_cast<uint8_t>(std::clamp(60 + x + n[0] + shift, 0, 255));
                        p[1] = static_cast<uint8_t>(std::clamp(90 + y * 2 + n[1], 0, 255));
                        p[2] = static_cast<uint8_t>(std::clamp(200 - x / 2 + n[2] - shift, 0, 255));
                    }
                }
            };
            Quantizer quantizer;
            std::vector<uint8_t> rgb;
            QuantizePalette palette;
            std::vector<uint8_t> first(w * h);
            fill(rgb, 0, 0);
            const size_t count = quantizer.quantize(rgb.data(), w * 3, w, h, first.data(), w, palette);
            FTK_CHECK(count > 0);
            FTK_CHECK(count < quantizeColorsMax);
            FTK_CHECK(0x00000000 == palette[quantizeColorsMax - 1]);
            FTK_CHECK(1 == quantizer.getPaletteCount());
            bool noTransparent = true;
            for (const uint8_t i : first)
            {
                noTransparent &= i < count;
            }
            FTK_CHECK(noTransparent);

            for (int frame = 1; frame <= 3; ++frame)
            {
                fill(rgb, frame, 0);
                QuantizePalette framePalette;
                std::vector<uint8_t> indices(w * h);
                quantizer.quantize(rgb.data(), w * 3, w, h, indices.data(), w, framePalette);
                FTK_CHECK(framePalette == palette);
                size_t same = 0;
                for (int i = 0; i < w * h; ++i)
                {
                    same += indices[i] == first[i] ? 1 : 0;
                }
                _print(ftk::Format("Noise frame {0}: {1}% of the pixels keep their color").
                    arg(frame).
                    arg(100.0 * same / (w * h)));
                FTK_CHECK(same > static_cast<size_t>(w * h) / 2);
            }
            FTK_CHECK(1 == quantizer.getPaletteCount());

            // A picture of other colors is given a palette of its own, and
            // its pixels their own colors.
            fill(rgb, 0, 120);
            QuantizePalette other;
            std::vector<uint8_t> indices(w * h);
            quantizer.quantize(rgb.data(), w * 3, w, h, indices.data(), w, other);
            FTK_CHECK(2 == quantizer.getPaletteCount());
            FTK_CHECK(other != palette);
            double error = 0.0;
            for (int i = 0; i < w * h; ++i)
            {
                const uint32_t c = other[indices[i]];
                const uint8_t* p = rgb.data() + i * 3;
                error +=
                    std::abs(static_cast<int>((c >> 16) & 0xFF) - p[0]) +
                    std::abs(static_cast<int>((c >> 8) & 0xFF) - p[1]) +
                    std::abs(static_cast<int>(c & 0xFF) - p[2]);
            }
            error /= static_cast<double>(w * h * 3);
            _print(ftk::Format("New picture: mean error {0}").arg(error));
            FTK_CHECK(error < 8.0);

            // And a frame of another size starts afresh.
            const uint8_t one[3] = { 10, 20, 30 };
            uint8_t index = 9;
            FTK_CHECK(1 == quantizer.quantize(one, 3, 1, 1, &index, 1, other));
            FTK_CHECK(0 == index);
            FTK_CHECK(0xFF0A141E == other[0]);
            FTK_CHECK(3 == quantizer.getPaletteCount());
        }

        void QuantizeTest::_empty()
        {
            QuantizePalette palette;
            uint8_t index = 7;
            const uint8_t rgb[3] = { 1, 2, 3 };
            FTK_CHECK(0 == quantize(nullptr, 3, 1, 1, &index, 1, palette));
            FTK_CHECK(0 == quantize(rgb, 3, 0, 1, &index, 1, palette));
            FTK_CHECK(7 == index);
            FTK_CHECK(1 == quantize(rgb, 3, 1, 1, &index, 1, palette));
            FTK_CHECK(0 == index);
            FTK_CHECK(0xFF010203 == palette[0]);
        }
    }
}
