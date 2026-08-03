// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CoreTest/HDRTest.h>

#include <tlRender/Core/HDR.h>

#include <ftk/Core/Assert.h>
#include <ftk/Core/Format.h>

namespace tl
{
    namespace core_tests
    {
        HDRTest::HDRTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "core_tests::HDRTest")
        {}

        std::shared_ptr<HDRTest> HDRTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<HDRTest>(new HDRTest(context));
        }

        void HDRTest::run()
        {
            _enums();
            _operators();
            _serialize();
        }

        void HDRTest::_enums()
        {
            FTK_TEST_ENUM(HDR_EOTF);
            FTK_TEST_ENUM(HDRPrimaries);
        }

        void HDRTest::_operators()
        {
            {
                HDRData a;
                HDRData b;
                FTK_CHECK(a == b);
                a.eotf = HDR_EOTF::ST2084;
                FTK_CHECK(a != b);
            }
        }

        void HDRTest::_serialize()
        {
            {
                HDRData value;
                value.eotf = HDR_EOTF::ST2084;
                value.primaries[0].x = .1F;
                value.primaries[0].y = .2F;
                value.primaries[1].x = .3F;
                value.primaries[1].y = .4F;
                value.primaries[2].x = .5F;
                value.primaries[2].y = .6F;
                value.primaries[3].x = .7F;
                value.primaries[3].y = .8F;
                value.displayMasteringLuminance = ftk::RangeF(.1F, .2F);
                value.maxCLL = .1F;
                value.maxFALL = .2F;
                nlohmann::json json;
                to_json(json, value);
                HDRData value2;
                from_json(json, value2);
                FTK_CHECK(value == value2);
            }
        }
    }
}
