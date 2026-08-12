// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/ForegroundOptionsTest.h>

#include <tlRender/Timeline/ForegroundOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>
#include <ftk/Core/String.h>

#include <nlohmann/json.hpp>

namespace tl
{
    namespace timeline_tests
    {
        ForegroundOptionsTest::ForegroundOptionsTest(
            const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::ForegroundOptionsTest")
        {}

        std::shared_ptr<ForegroundOptionsTest> ForegroundOptionsTest::create(
            const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<ForegroundOptionsTest>(
                new ForegroundOptionsTest(context));
        }

        void ForegroundOptionsTest::run()
        {
            {
                FTK_TEST_ENUM(GridCellMode);
                FTK_TEST_ENUM(GridLabels);
            }
            {
                // A field at a time, which is what notices one left out of
                // the comparison.
                Grid v;
                FTK_CHECK(v == Grid());
                v = Grid();
                v.enabled = true;
                FTK_CHECK(v != Grid());
                v = Grid();
                v.cellMode = GridCellMode::CellCount;
                FTK_CHECK(v != Grid());
                v = Grid();
                v.cellSize = 1;
                FTK_CHECK(v != Grid());
                v = Grid();
                v.cellCount = ftk::V2I(1, 1);
                FTK_CHECK(v != Grid());
                v = Grid();
                v.lineWidth = 1;
                FTK_CHECK(v != Grid());
                v = Grid();
                v.color = ftk::Color4F(1.F, 1.F, 1.F);
                FTK_CHECK(v != Grid());
                v = Grid();
                v.labels = GridLabels::Pixels;
                FTK_CHECK(v != Grid());
            }
            {
                CenterMarker v;
                FTK_CHECK(v == CenterMarker());
                v = CenterMarker();
                v.enabled = true;
                FTK_CHECK(v != CenterMarker());
                v = CenterMarker();
                v.size = 1;
                FTK_CHECK(v != CenterMarker());
                v = CenterMarker();
                v.width = 1;
                FTK_CHECK(v != CenterMarker());
                v = CenterMarker();
                v.color = ftk::Color4F(1.F, 0.F, 0.F);
                FTK_CHECK(v != CenterMarker());
            }
            {
                MissingIndicator v;
                FTK_CHECK(v == MissingIndicator());
                v = MissingIndicator();
                v.enabled = true;
                FTK_CHECK(v != MissingIndicator());
                v = MissingIndicator();
                v.width = 1;
                FTK_CHECK(v != MissingIndicator());
                v = MissingIndicator();
                v.color = ftk::Color4F(0.F, 1.F, 0.F);
                FTK_CHECK(v != MissingIndicator());
            }
            {
                ForegroundOptions v;
                FTK_CHECK(v == ForegroundOptions());
                v = ForegroundOptions();
                v.grid.enabled = true;
                FTK_CHECK(v != ForegroundOptions());
                v = ForegroundOptions();
                v.centerMarker.enabled = true;
                FTK_CHECK(v != ForegroundOptions());
                v = ForegroundOptions();
                v.missingIndicator.enabled = true;
                FTK_CHECK(v != ForegroundOptions());
            }
            {
                ForegroundOptions v;
                v.grid.enabled = true;
                v.grid.cellMode = GridCellMode::CellCount;
                v.grid.cellCount = ftk::V2I(4, 4);
                v.grid.labels = GridLabels::Pixels;
                v.centerMarker.enabled = true;
                v.centerMarker.size = 40;
                v.missingIndicator.enabled = true;
                v.missingIndicator.width = 8;
                nlohmann::json json;
                to_json(json, v);
                ForegroundOptions v2;
                from_json(json, v2);
                FTK_CHECK(v == v2);
            }
        }
    }
}
