// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/Read.h>

#include <ftk/TestLib/ITest.h>

namespace tl
{
    namespace io_tests
    {
        class SVGTest : public ftk::test::ITest
        {
        protected:
            SVGTest(const std::shared_ptr<ftk::Context>&);

        public:
            static std::shared_ptr<SVGTest> create(const std::shared_ptr<ftk::Context>&);

            void run() override;

        private:
            //! Write the given text to a file in the temporary directory.
            ftk::Path _write(const std::string& fileName, const std::string&);

            //! Read one file and return the image, going through memory when
            //! asked so both paths are covered.
            std::shared_ptr<ftk::Image> _read(
                const std::shared_ptr<IReadPlugin>&,
                const ftk::Path&,
                bool memoryIO,
                const IOOptions&);
        };
    }
}
