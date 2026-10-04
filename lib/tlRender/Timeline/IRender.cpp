// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Timeline/IRender.h>

#include <tlRender/Timeline/RenderPrivate.h>

#include <ftk/Core/Format.h>
#include <ftk/Core/Math.h>

#include <cmath>

namespace tl
{
    IRender::~IRender()
    {}

    void IRender::_drawBackground(
        const std::vector<ftk::Box2I>& boxes,
        const ftk::M44F& vm,
        const BackgroundOptions& options,
        const CompareOptions& compareOptions,
        const std::function<void()>& blend)
    {
        if (blend)
        {
            blend();
        }

        // Draw the background.
        const ftk::Box2I rect(ftk::V2I(0, 0), getRenderSize());
        switch (options.type)
        {
        case Background::Solid:
            IRender::drawRect(rect, options.solidColor);
            break;
        case Background::Checkers:
            drawColorMesh(
                ftk::checkers(
                    rect,
                    options.checkersColor.first,
                    options.checkersColor.second,
                    options.checkersSize),
                ftk::Color4F(1.F, 1.F, 1.F));
            break;
        case Background::Gradient:
        {
            ftk::TriMesh2F mesh;
            mesh.v.push_back(ftk::V2F(rect.min.x, rect.min.y));
            mesh.v.push_back(ftk::V2F(rect.max.x, rect.min.y));
            mesh.v.push_back(ftk::V2F(rect.max.x, rect.max.y));
            mesh.v.push_back(ftk::V2F(rect.min.x, rect.max.y));
            mesh.c.push_back(ftk::V4F(
                options.gradientColor.first.r,
                options.gradientColor.first.g,
                options.gradientColor.first.b,
                options.gradientColor.first.a));
            mesh.c.push_back(ftk::V4F(
                options.gradientColor.second.r,
                options.gradientColor.second.g,
                options.gradientColor.second.b,
                options.gradientColor.second.a));
            mesh.triangles.push_back({
                ftk::Vertex2(1, 0, 1),
                ftk::Vertex2(3, 0, 2),
                ftk::Vertex2(2, 0, 1), });
            mesh.triangles.push_back({
                ftk::Vertex2(1, 0, 1),
                ftk::Vertex2(4, 0, 2),
                ftk::Vertex2(3, 0, 2), });
            drawColorMesh(
                mesh,
                ftk::Color4F(1.F, 1.F, 1.F));
            break;
        }
        default: break;
        }

        // Draw the outline.
        if (options.outline.enabled && !boxes.empty())
        {
            if (blend)
            {
                blend();
            }

            for (size_t i = 0; i < boxes.size(); ++i)
            {
                if (!isShown(compareOptions.compare, i))
                {
                    continue;
                }
                const ftk::Box2I box = xform(boxes[i], vm);

                ftk::TriMesh2F mesh;
                mesh.v.push_back(ftk::V2F(box.min.x, box.min.y));
                mesh.v.push_back(ftk::V2F(box.max.x + 1, box.min.y));
                mesh.v.push_back(ftk::V2F(box.max.x + 1, box.max.y + 1));
                mesh.v.push_back(ftk::V2F(box.min.x, box.max.y + 1));
                const int w = options.outline.width;
                mesh.v.push_back(ftk::V2F(box.min.x - w, box.min.y - w));
                mesh.v.push_back(ftk::V2F(box.max.x + 1 + w, box.min.y - w));
                mesh.v.push_back(ftk::V2F(box.max.x + 1 + w, box.max.y + 1 + w));
                mesh.v.push_back(ftk::V2F(box.min.x - w, box.max.y + 1 + w));

                mesh.triangles.push_back({ ftk::Vertex2(1), ftk::Vertex2(2), ftk::Vertex2(5) });
                mesh.triangles.push_back({ ftk::Vertex2(2), ftk::Vertex2(6), ftk::Vertex2(5) });
                mesh.triangles.push_back({ ftk::Vertex2(2), ftk::Vertex2(3), ftk::Vertex2(6) });
                mesh.triangles.push_back({ ftk::Vertex2(3), ftk::Vertex2(7), ftk::Vertex2(6) });
                mesh.triangles.push_back({ ftk::Vertex2(3), ftk::Vertex2(4), ftk::Vertex2(7) });
                mesh.triangles.push_back({ ftk::Vertex2(4), ftk::Vertex2(8), ftk::Vertex2(7) });
                mesh.triangles.push_back({ ftk::Vertex2(4), ftk::Vertex2(1), ftk::Vertex2(8) });
                mesh.triangles.push_back({ ftk::Vertex2(1), ftk::Vertex2(5), ftk::Vertex2(8) });

                drawMesh(mesh, options.outline.color);
            }
        }
    }

    void IRender::_drawForeground(
        const std::vector<ftk::Box2I>& boxes,
        const ftk::M44F& vm,
        const ForegroundOptions& options,
        const CompareOptions& compareOptions,
        const std::function<void()>& blend)
    {
        size_t start = 0;
        size_t end = boxes.size();
        switch (compareOptions.compare)
        {
            case Compare::None:
            case Compare::Wipe:
            case Compare::Butterfly:
            case Compare::Overlay:
            case Compare::Difference:
                if (!boxes.empty())
                {
                    end = 1;
                }
                break;
            case Compare::B:
                if (boxes.size() > 1)
                {
                    start = 1;
                    end = 2;
                }
                break;
            default: break;
        }
        for (size_t i = start; i < end; ++i)
        {
            const ftk::Box2I& box = boxes[i];
            const ftk::Box2I boxT = xform(box, vm);

            if (options.grid.enabled &&
                GridCellMode::CellSize == options.grid.cellMode)
            {
                if (blend)
                {
                    blend();
                }

                const ftk::Size2I cellSizeT(
                    ftk::length(
                        vm * ftk::V3F(0.F, 0.F, 0.F) -
                        vm * ftk::V3F(options.grid.cellSize, 0.F, 0.F)),
                    ftk::length(
                        vm * ftk::V3F(0.F, 0.F, 0.F) -
                        vm * ftk::V3F(0.F, options.grid.cellSize, 0.F)));

                if (cellSizeT.w > options.grid.lineWidth + 10.F &&
                    cellSizeT.h > options.grid.lineWidth + 10.F)
                {
                    const ftk::Size2I& renderSize = getRenderSize();
                    ftk::M44F mi;
                    ftk::invert(vm, mi);
                    const ftk::V3F v0 = mi * ftk::V3F(0.F, 0.F, 0.F);
                    const ftk::V3F v1 = mi * ftk::V3F(renderSize.w, renderSize.h, 0.F);
                    const ftk::V2F v2(
                        std::max(static_cast<int>(v0.x) / options.grid.cellSize * options.grid.cellSize, box.min.x),
                        std::max(static_cast<int>(v0.y) / options.grid.cellSize * options.grid.cellSize, box.min.y));
                    const ftk::V2F v3(
                        std::min(static_cast<int>(v1.x) / options.grid.cellSize * options.grid.cellSize, box.max.x),
                        std::min(static_cast<int>(v1.y) / options.grid.cellSize * options.grid.cellSize, box.max.y));

                    if (options.grid.labels != GridLabels::None)
                    {
                        auto fontSystem = _fontSystem.lock();
                        const ftk::FontMetrics fontMetrics = fontSystem->getMetrics(options.grid.fontInfo);
                        std::string text = getLabel(
                            options.grid.labels,
                            GridLabels::Pixels == options.grid.labels ? v3.x : v3.x / options.grid.cellSize,
                            GridLabels::Pixels == options.grid.labels ? v3.y : v3.y / options.grid.cellSize);
                        ftk::Size2I size =
                            fontSystem->getSize(text, options.grid.fontInfo) +
                            options.grid.textMargin * 2;
                        if (size.w <= cellSizeT.w - options.grid.lineWidth &&
                            size.h <= cellSizeT.h - options.grid.lineWidth)
                        {
                            for (int y = v2.y, i = v2.y / options.grid.cellSize;
                                y <= v3.y + 1;
                                y += options.grid.cellSize, ++i)
                            {
                                for (int x = v2.x, j = v2.x / options.grid.cellSize;
                                    x <= v3.x + 1;
                                    x += options.grid.cellSize, ++j)
                                {
                                    text = getLabel(
                                        options.grid.labels,
                                        GridLabels::Pixels == options.grid.labels ? (x - box.min.x) : j,
                                        GridLabels::Pixels == options.grid.labels ? (y - box.min.y) : i);
                                    size =
                                        fontSystem->getSize(text, options.grid.fontInfo) +
                                        options.grid.textMargin * 2;
                                    const ftk::V3F v4 = vm * ftk::V3F(x, y, 0.F);
                                    const ftk::V2F v5(v4.x, v4.y);
                                    if (v5.x + options.grid.lineWidth / 2.F + size.w <= boxT.max.x &&
                                        v5.y + options.grid.lineWidth / 2.F + size.h <= boxT.max.y)
                                    {
                                        drawRect(
                                            ftk::Box2F(
                                                v5.x + options.grid.lineWidth / 2.F,
                                                v5.y + options.grid.lineWidth / 2.F,
                                                size.w,
                                                size.h),
                                            options.grid.overlayColor);
                                        drawText(
                                            fontSystem->getGlyphs(text, options.grid.fontInfo),
                                            fontMetrics,
                                            ftk::V2F(
                                                v5.x + options.grid.lineWidth / 2.F + options.grid.textMargin,
                                                v5.y + options.grid.lineWidth / 2.F + options.grid.textMargin),
                                            options.grid.textColor);
                                    }
                                }
                            }
                        }
                    }

                    std::vector<ftk::Box2F> rects;
                    for (int y = v2.y, i = v2.y / options.grid.cellSize;
                        y <= v3.y + 1;
                        y += options.grid.cellSize, ++i)
                    {
                        const ftk::V3F v0 = vm * ftk::V3F(box.min.x, y, 0.F);
                        const ftk::V3F v1 = vm * ftk::V3F(box.max.x + 1, y, 0.F);
                        const ftk::V2I v2(
                            ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                        const ftk::V2I v3(
                            ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                        rects.push_back(ftk::Box2F(
                            v2.x,
                            v2.y - options.grid.lineWidth / 2,
                            v3.x - v2.x + 1,
                            options.grid.lineWidth));
                    }
                    for (int x = v2.x, j = v2.x / options.grid.cellSize;
                        x <= v3.x + 1;
                        x += options.grid.cellSize, ++j)
                    {
                        const ftk::V3F v0 = vm * ftk::V3F(x, box.min.y, 0.F);
                        const ftk::V3F v1 = vm * ftk::V3F(x, box.max.y + 1, 0.F);
                        const ftk::V2I v2(
                            ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                        const ftk::V2I v3(
                            ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                        rects.push_back(ftk::Box2F(
                            v2.x - options.grid.lineWidth / 2,
                            v2.y,
                            options.grid.lineWidth,
                            v3.y - v2.y + 1));
                    }
                    drawRects(rects, options.grid.color);
                }
            }

            if (options.grid.enabled &&
                GridCellMode::CellCount == options.grid.cellMode)
            {
                if (blend)
                {
                    blend();
                }

                const ftk::V2I cellCount(
                    std::max(1, options.grid.cellCount.x),
                    std::max(1, options.grid.cellCount.y));

                const ftk::Size2I cellSize(
                    box.w() / static_cast<float>(cellCount.x),
                    box.h() / static_cast<float>(cellCount.y));
                const ftk::Size2I cellSizeT(
                    ftk::length(
                        vm * ftk::V3F(0.F, 0.F, 0.F) -
                        vm * ftk::V3F(cellSize.w, 0.F, 0.F)),
                    ftk::length(
                        vm * ftk::V3F(0.F, 0.F, 0.F) -
                        vm * ftk::V3F(0.F, cellSize.h, 0.F)));

                if (cellSizeT.w > options.grid.lineWidth + 10.F &&
                    cellSizeT.h > options.grid.lineWidth + 10.F)
                {
                    if (options.grid.labels != GridLabels::None)
                    {
                        auto fontSystem = _fontSystem.lock();
                        const ftk::FontMetrics fontMetrics = fontSystem->getMetrics(options.grid.fontInfo);
                        const ftk::V3F v1 = vm * ftk::V3F(box.w(), box.h(), 0.F);
                        std::string text = getLabel(
                            options.grid.labels,
                            GridLabels::Pixels == options.grid.labels ? v1.x : v1.x / cellSize.w,
                            GridLabels::Pixels == options.grid.labels ? v1.y : v1.y / cellSize.h);
                        ftk::Size2I size =
                            fontSystem->getSize(text, options.grid.fontInfo) +
                            options.grid.textMargin * 2;
                        if (size.w <= cellSizeT.w - options.grid.lineWidth &&
                            size.h <= cellSizeT.h - options.grid.lineWidth)
                        {
                            for (int i = 0; i < options.grid.cellCount.y; ++i)
                            {
                                const int y = box.min.y + i / static_cast<float>(options.grid.cellCount.y) * box.h();
                                for (int j = 0; j < options.grid.cellCount.x; ++j)
                                {
                                    const int x = box.min.x + j / static_cast<float>(options.grid.cellCount.x) * box.w();
                                    text = getLabel(
                                        options.grid.labels,
                                        GridLabels::Pixels == options.grid.labels ? (x - box.min.x) : j,
                                        GridLabels::Pixels == options.grid.labels ? (y - box.min.y) : i);
                                    size =
                                        fontSystem->getSize(text, options.grid.fontInfo) +
                                        options.grid.textMargin * 2;
                                    const ftk::V3F v2 = vm * ftk::V3F(x, y, 0.F);
                                    const ftk::V2F v3(
                                        ftk::clamp(static_cast<int>(v2.x), boxT.min.x, boxT.max.x),
                                        ftk::clamp(static_cast<int>(v2.y), boxT.min.y, boxT.max.y));
                                    drawRect(
                                        ftk::Box2F(
                                            v3.x + options.grid.lineWidth / 2,
                                            v3.y + options.grid.lineWidth / 2,
                                            size.w,
                                            size.h),
                                        options.grid.overlayColor);
                                    drawText(
                                        fontSystem->getGlyphs(text, options.grid.fontInfo),
                                        fontMetrics,
                                        ftk::V2F(
                                            v3.x + options.grid.lineWidth / 2 + options.grid.textMargin,
                                            v3.y + options.grid.lineWidth / 2 + options.grid.textMargin),
                                        options.grid.textColor);
                                }
                            }
                        }
                    }

                    std::vector<ftk::Box2F> rects;
                    for (int i = 0; i <= cellCount.y; ++i)
                    {
                        const float y = box.min.y + i / static_cast<float>(options.grid.cellCount.y) * box.h();
                        const ftk::V3F v0 = vm * ftk::V3F(box.min.x, y, 0.F);
                        const ftk::V3F v1 = vm * ftk::V3F(box.max.x + 1, y, 0.F);
                        const ftk::V2I v2(
                            ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                        const ftk::V2I v3(
                            ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                        rects.push_back(ftk::Box2F(
                            v2.x,
                            v2.y - options.grid.lineWidth / 2,
                            boxT.w(),
                            options.grid.lineWidth));
                    }
                    for (int i = 0; i <= cellCount.x; ++i)
                    {
                        const float x = box.min.x + i / static_cast<float>(options.grid.cellCount.x) * box.w();
                        const ftk::V3F v0 = vm * ftk::V3F(x, box.min.y, 0.F);
                        const ftk::V3F v1 = vm * ftk::V3F(x, box.max.y + 1, 0.F);
                        const ftk::V2I v2(
                            ftk::clamp(static_cast<int>(v0.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v0.y), boxT.min.y, boxT.max.y));
                        const ftk::V2I v3(
                            ftk::clamp(static_cast<int>(v1.x), boxT.min.x, boxT.max.x),
                            ftk::clamp(static_cast<int>(v1.y), boxT.min.y, boxT.max.y));
                        rects.push_back(ftk::Box2F(
                            v2.x - options.grid.lineWidth / 2,
                            v2.y,
                            options.grid.lineWidth,
                            boxT.h()));
                    }
                    drawRects(rects, options.grid.color);
                }
            }

            if (options.centerMarker.enabled)
            {
                if (blend)
                {
                    blend();
                }
                std::vector<ftk::Box2F> centerMarker;
                const ftk::V2F c(
                    box.x() + box.w() / 2.F,
                    box.y() + box.h() / 2.F);
                const ftk::V3F v = vm * ftk::V3F(c.x, c.y, 0.F);
                const float a = 1.F / 3.F;
                const float b = 2.F / 3.F;
                centerMarker.push_back(ftk::Box2F(
                    v.x - options.centerMarker.width / 2,
                    v.y - options.centerMarker.size,
                    options.centerMarker.width,
                    options.centerMarker.size * b));
                centerMarker.push_back(ftk::Box2F(
                    v.x - options.centerMarker.width / 2,
                    v.y + options.centerMarker.size * a,
                    options.centerMarker.width,
                    options.centerMarker.size * b));
                centerMarker.push_back(ftk::Box2F(
                    v.x - options.centerMarker.size,
                    v.y - options.centerMarker.width / 2,
                    options.centerMarker.size * b,
                    options.centerMarker.width));
                centerMarker.push_back(ftk::Box2F(
                    v.x + options.centerMarker.size * a,
                    v.y - options.centerMarker.width / 2,
                    options.centerMarker.size * b,
                    options.centerMarker.width));
                drawRects(centerMarker, options.centerMarker.color);
            }
        }
    }
}
