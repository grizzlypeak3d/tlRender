// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/IO/EXR.h>

#include <ftk/Core/Box.h>

#include <ImathBox.h>
#include <ImfHeader.h>

//! The OpenEXR version as one number, major * 10000 + minor * 100 + patch.
//! OPENEXR_VERSION_HEX is not used: 3.1 defines it with a cast, which the
//! preprocessor cannot evaluate in a condition.
#define TLRENDER_OPENEXR_VERSION \
    (OPENEXR_VERSION_MAJOR * 10000 + \
     OPENEXR_VERSION_MINOR * 100 + \
     OPENEXR_VERSION_PATCH)
#include <ImfPixelType.h>

namespace tl
{
    namespace exr
    {
        //! Read the tags from an Imf header.
        void readTags(const Imf::Header&, ftk::ImageTags&);

        //! The tags every part of a multipart file has its own of, which
        //! are given for each part whether or not they differ from the
        //! first part's.
        const std::vector<std::string>& getPartHeaderTags();

        //! Read the tags of the parts after the first, given the first
        //! part's: a part's tags are prefixed "Part N/", and are the header
        //! tags and those that differ from the first part's. "Parts" is the
        //! number of parts. Nothing is added for a file of one part.
        void readPartTags(const std::vector<const Imf::Header*>&, ftk::ImageTags&);

        //! Write tags to an Imf header.
        void writeTags(const ftk::ImageTags&, double speed, Imf::Header&);

        //! Convert to Imf.
        Imf::Compression toImf(Compression);

        //! Convert from Imath.
        ftk::Box2I fromImath(const Imath::Box2i&);

        //! Input stream.
        class IStream : public Imf::IStream
        {
            FTK_NON_COPYABLE(IStream);

        public:
            IStream(const std::string& fileName);
            IStream(const std::string& fileName, const uint8_t*, size_t);

            virtual ~IStream();

            bool isMemoryMapped() const override;
            char* readMemoryMapped(int n) override;
            bool read(char c[], int n) override;
            uint64_t tellg() override;
            void seekg(uint64_t pos) override;

        private:
            FTK_PRIVATE();
        };
   }
}
