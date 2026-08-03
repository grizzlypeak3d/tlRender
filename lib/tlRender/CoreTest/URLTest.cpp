// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/CoreTest/URLTest.h>

#include <tlRender/Core/URL.h>

#include <ftk/Core/Assert.h>

#include <sstream>
#include <vector>

namespace tl
{
    namespace core_tests
    {
        URLTest::URLTest(const std::shared_ptr<ftk::Context>& context) :
            ITest(context, "core_tests::URLTest")
        {}

        std::shared_ptr<URLTest> URLTest::create(const std::shared_ptr<ftk::Context>& context)
        {
            return std::shared_ptr<URLTest>(new URLTest(context));
        }

        void URLTest::run()
        {
            _util();
            _encode();
        }
                
        void URLTest::_util()
        {
            {
                const std::string scheme = getURLScheme("file://path");
                FTK_CHECK("file://" == scheme);
            }
            {
                const std::string scheme = getURLScheme("path");
                FTK_CHECK(scheme.empty());
            }
        }
        
        void URLTest::_encode()
        {
            struct Data
            {
                std::string encoded;
                std::string decoded;
            };
            const std::vector<Data> data =
            {
                { "NoSpaces", "NoSpaces" },
                { "With%20Spaces", "With Spaces" },
                { "%20With%20Spaces", " With Spaces" },
                { "With%20Spaces%20", "With Spaces " },
                { "%20With%20Spaces%20", " With Spaces " }
            };
            for (const auto& i : data)
            {
                const std::string decoded = decodeURL(i.encoded);
                FTK_CHECK(decoded == i.decoded);
                const std::string encoded = encodeURL(decoded);
                FTK_CHECK(encoded == i.encoded);
            }
        }
    }
}
