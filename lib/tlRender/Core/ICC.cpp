// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/ICC.h>

#include <array>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>

namespace tl
{
    namespace
    {
        typedef std::array<double, 3> V3;
        typedef std::array<V3, 3> M3;

        V3 mul(const M3& m, const V3& v)
        {
            return {
                m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
                m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
                m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2] };
        }

        M3 mul(const M3& a, const M3& b)
        {
            M3 out{};
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    for (int k = 0; k < 3; ++k)
                        out[i][j] += a[i][k] * b[k][j];
            return out;
        }

        M3 inverse(const M3& m)
        {
            const double det =
                m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
                m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
                m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
            M3 out{};
            out[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / det;
            out[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / det;
            out[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / det;
            out[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) / det;
            out[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / det;
            out[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / det;
            out[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / det;
            out[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / det;
            out[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / det;
            return out;
        }

        //! A chromaticity as XYZ with Y one.
        V3 xyz(double x, double y)
        {
            return { x / y, 1.0, (1.0 - x - y) / y };
        }

        //! Red, green, blue and white x and y.
        typedef std::array<double, 8> Chromaticities;

        std::optional<Chromaticities> getChromaticities(const ftk::ImageTags& tags)
        {
            if (auto i = tags.find("Chromaticities"); i != tags.end())
            {
                Chromaticities out{};
                std::istringstream s(i->second);
                for (auto& v : out)
                {
                    s >> v;
                }
                if (!s.fail() && out[1] > 0.0 && out[3] > 0.0 && out[5] > 0.0 && out[7] > 0.0)
                {
                    return out;
                }
                return std::nullopt;
            }
            if (auto i = tags.find("Color Primaries"); i != tags.end())
            {
                if ("bt709" == i->second)
                    return Chromaticities{ .64, .33, .3, .6, .15, .06, .3127, .329 };
                if ("smpte432" == i->second)
                    return Chromaticities{ .68, .32, .265, .69, .15, .06, .3127, .329 };
                if ("bt2020" == i->second)
                    return Chromaticities{ .708, .292, .17, .797, .131, .046, .3127, .329 };
            }
            return std::nullopt;
        }

        //! A curve as ICC's parametric curve: its function type and its
        //! parameters.
        struct Curve
        {
            uint16_t type = 0;
            std::vector<double> params;
            std::string name;
        };

        std::optional<Curve> getCurve(const ftk::ImageTags& tags)
        {
            auto i = tags.find("Color Transfer");
            if (i == tags.end())
                return std::nullopt;
            if ("iec61966-2-1" == i->second)
            {
                return Curve{ 3, { 2.4, 1.0 / 1.055, .055 / 1.055, 1.0 / 12.92, .04045 }, "sRGB curve" };
            }
            if ("bt709" == i->second || "bt1886" == i->second)
            {
                return Curve{ 0, { 2.4 }, "gamma 2.4" };
            }
            if ("gamma22" == i->second)
            {
                return Curve{ 0, { 2.2 }, "gamma 2.2" };
            }
            return std::nullopt;
        }

        std::string getName(const Chromaticities& c)
        {
            const auto near = [&c](const Chromaticities& d)
            {
                for (size_t i = 0; i < c.size(); ++i)
                {
                    if (std::abs(c[i] - d[i]) > .002)
                        return false;
                }
                return true;
            };
            if (near({ .64, .33, .3, .6, .15, .06, .3127, .329 }))
                return "Rec.709";
            if (near({ .68, .32, .265, .69, .15, .06, .3127, .329 }))
                return "P3-D65";
            if (near({ .708, .292, .17, .797, .131, .046, .3127, .329 }))
                return "Rec.2020";
            return "RGB";
        }

        class Writer
        {
        public:
            std::vector<uint8_t> data;

            void u8(uint8_t v) { data.push_back(v); }
            void u16(uint16_t v) { u8(v >> 8); u8(v & 0xff); }
            void u32(uint32_t v) { u16(v >> 16); u16(v & 0xffff); }
            void sig(const char* s) { for (int i = 0; i < 4; ++i) u8(s[i]); }
            void s15(double v) { u32(static_cast<uint32_t>(static_cast<int32_t>(std::lround(v * 65536.0)))); }
            void pad() { while (data.size() % 4) u8(0); }
            void set32(size_t at, uint32_t v)
            {
                data[at] = v >> 24;
                data[at + 1] = (v >> 16) & 0xff;
                data[at + 2] = (v >> 8) & 0xff;
                data[at + 3] = v & 0xff;
            }
        };

        //! A multi-localized Unicode text, in English.
        std::vector<uint8_t> mluc(const std::string& text)
        {
            Writer w;
            w.sig("mluc");
            w.u32(0);
            w.u32(1);
            w.u32(12);
            w.u16('e' << 8 | 'n');
            w.u16('U' << 8 | 'S');
            w.u32(static_cast<uint32_t>(text.size() * 2));
            w.u32(28);
            for (const char c : text)
            {
                w.u16(static_cast<uint8_t>(c));
            }
            return w.data;
        }

        std::vector<uint8_t> xyzTag(const V3& v)
        {
            Writer w;
            w.sig("XYZ ");
            w.u32(0);
            for (const double c : v)
            {
                w.s15(c);
            }
            return w.data;
        }
    }

    std::vector<uint8_t> getICCProfile(const ftk::ImageTags& tags)
    {
        const auto chromaticities = getChromaticities(tags);
        const auto curve = getCurve(tags);
        if (!chromaticities || !curve)
        {
            return {};
        }
        const Chromaticities& c = *chromaticities;

        // RGB to XYZ, each primary scaled so that the three make the white;
        // then taken from that white to the D50 of the connection space,
        // with Bradford's adaptation, which the "chad" tag records.
        const V3 r = xyz(c[0], c[1]);
        const V3 g = xyz(c[2], c[3]);
        const V3 b = xyz(c[4], c[5]);
        const V3 white = xyz(c[6], c[7]);
        const M3 p = { V3{ r[0], g[0], b[0] }, V3{ r[1], g[1], b[1] }, V3{ r[2], g[2], b[2] } };
        const V3 s = mul(inverse(p), white);
        M3 rgbToXYZ = p;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                rgbToXYZ[i][j] *= s[j];
        const V3 d50 = { .9642, 1.0, .8249 };
        const M3 bradford =
        {
            V3{ .8951, .2664, -.1614 },
            V3{ -.7502, 1.7135, .0367 },
            V3{ .0389, -.0685, 1.0296 }
        };
        const V3 from = mul(bradford, white);
        const V3 to = mul(bradford, d50);
        M3 scale{};
        for (int i = 0; i < 3; ++i)
        {
            scale[i][i] = to[i] / from[i];
        }
        const M3 adapt = mul(inverse(bradford), mul(scale, bradford));
        const M3 m = mul(adapt, rgbToXYZ);

        // The tags' data.
        struct Tag
        {
            const char* sig;
            std::vector<uint8_t> data;
        };
        std::vector<Tag> tagData;
        tagData.push_back({ "desc", mluc(getName(c) + ", " + curve->name) });
        tagData.push_back({ "cprt", mluc("No copyright, use freely") });
        tagData.push_back({ "wtpt", xyzTag(d50) });
        {
            Writer w;
            w.sig("sf32");
            w.u32(0);
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    w.s15(adapt[i][j]);
            tagData.push_back({ "chad", w.data });
        }
        tagData.push_back({ "rXYZ", xyzTag({ m[0][0], m[1][0], m[2][0] }) });
        tagData.push_back({ "gXYZ", xyzTag({ m[0][1], m[1][1], m[2][1] }) });
        tagData.push_back({ "bXYZ", xyzTag({ m[0][2], m[1][2], m[2][2] }) });
        {
            Writer w;
            w.sig("para");
            w.u32(0);
            w.u16(curve->type);
            w.u16(0);
            for (const double v : curve->params)
            {
                w.s15(v);
            }
            tagData.push_back({ "rTRC", w.data });
        }

        // The header, the tag table, and the tags; the three curves are
        // the one, which the table may share.
        Writer w;
        w.u32(0);
        w.u32(0);
        w.u32(0x04300000);
        w.sig("mntr");
        w.sig("RGB ");
        w.sig("XYZ ");
        for (const uint16_t v : { 2026, 1, 1, 0, 0, 0 })
        {
            w.u16(v);
        }
        w.sig("acsp");
        for (int i = 0; i < 7; ++i)
        {
            // Platform, flags, manufacturer, model, attributes (eight
            // bytes), and the perceptual intent.
            w.u32(0);
        }
        for (const double v : d50)
        {
            w.s15(v);
        }
        while (w.data.size() < 128)
        {
            w.u8(0);
        }
        const size_t count = tagData.size() + 2;
        w.u32(static_cast<uint32_t>(count));
        const size_t tableAt = w.data.size();
        for (size_t i = 0; i < count * 12; ++i)
        {
            w.u8(0);
        }
        std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t> > > table;
        for (const auto& tag : tagData)
        {
            w.pad();
            const uint32_t offset = static_cast<uint32_t>(w.data.size());
            w.data.insert(w.data.end(), tag.data.begin(), tag.data.end());
            table.push_back({ tag.sig, { offset, static_cast<uint32_t>(tag.data.size()) } });
        }
        table.push_back({ "gTRC", table.back().second });
        table.push_back({ "bTRC", table.back().second });
        w.pad();
        for (size_t i = 0; i < table.size(); ++i)
        {
            const size_t at = tableAt + i * 12;
            for (int k = 0; k < 4; ++k)
            {
                w.data[at + k] = table[i].first[k];
            }
            w.set32(at + 4, table[i].second.first);
            w.set32(at + 8, table[i].second.second);
        }
        w.set32(0, static_cast<uint32_t>(w.data.size()));
        return w.data;
    }
}
