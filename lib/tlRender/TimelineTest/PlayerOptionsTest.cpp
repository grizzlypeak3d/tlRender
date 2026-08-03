// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/TimelineTest/PlayerOptionsTest.h>

#include <tlRender/Timeline/PlayerOptions.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/String.h>

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
        }
    }
}
