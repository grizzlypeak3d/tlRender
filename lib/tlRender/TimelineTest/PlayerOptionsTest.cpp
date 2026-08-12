// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/PlayerOptionsTest.h>

#include <tlRender/Timeline/PlayerOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/String.h>

#include <nlohmann/json.hpp>

namespace tl
{
    namespace timeline_tests
    {
        PlayerOptionsTest::PlayerOptionsTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "timeline_tests::PlayerOptionsTest")
        {}

        std::shared_ptr<PlayerOptionsTest> PlayerOptionsTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<PlayerOptionsTest>(new PlayerOptionsTest(context));
        }

        void PlayerOptionsTest::run()
        {
            {
                PlayerCacheOptions v;
                v.readBehind = 0.F;
                FTK_CHECK(v == v);
                FTK_CHECK(v != PlayerCacheOptions());
            }
            {
                // Each field on its own, so that one left out of the
                // comparison is noticed.
                PlayerOptions v;
                FTK_CHECK(v == PlayerOptions());
                v = PlayerOptions();
                v.cache.videoGB = 1.F;
                FTK_CHECK(v != PlayerOptions());
                v = PlayerOptions();
                v.videoRequestMax = 1;
                FTK_CHECK(v != PlayerOptions());
                v = PlayerOptions();
                v.audioRequestMax = 1;
                FTK_CHECK(v != PlayerOptions());
                v = PlayerOptions();
                v.audioBufferFrameCount = 1;
                FTK_CHECK(v != PlayerOptions());
                v = PlayerOptions();
                v.muteTimeout = std::chrono::milliseconds(1);
                FTK_CHECK(v != PlayerOptions());
                v = PlayerOptions();
                v.sleepTimeout = std::chrono::milliseconds(1);
                FTK_CHECK(v != PlayerOptions());
                v = PlayerOptions();
                v.currentTime = OTIO_NS::RationalTime(1.0, 24.0);
                FTK_CHECK(v != PlayerOptions());
            }
            {
                PlayerCacheOptions v;
                v.videoGB = 8.F;
                v.audioGB = 1.F;
                v.readBehind = 2.F;
                nlohmann::json json;
                to_json(json, v);
                PlayerCacheOptions v2;
                from_json(json, v2);
                FTK_CHECK(v == v2);
            }
        }
    }
}
