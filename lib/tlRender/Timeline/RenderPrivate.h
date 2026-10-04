// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/Export.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Matrix.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#include <OpenColorIO/OpenColorTransforms.h>
#endif // TLRENDER_OCIO

#include <string>

namespace tl
{
    //! \name Renderers
    //! What a timeline renderer does that is the same whatever it draws
    //! with: tl::gl::Render and tl::gpu::Render both use these. The drawing
    //! they share is in IRender.
    ///@{

    //! Get a box transformed by a matrix, to whole pixels.
    TL_TIMELINE_API ftk::Box2I xform(const ftk::Box2I&, const ftk::M44F&);

#if defined(TLRENDER_OCIO)
    //! Get the configuration the options name, read once. Reading one
    //! parses a whole YAML document -- the built in configuration included
    //! -- and OCIO keeps its processor cache on the object, so building a
    //! second one throws that away as well.
    //!
    //! A configuration that comes from a file is remembered with that
    //! file's size and write time, so editing it is still picked up.
    TL_TIMELINE_API OCIO_NAMESPACE::ConstConfigRcPtr getOCIOConfig(const OCIOOptions&);

    //! Get what the data and shaders built from a set of options are keyed
    //! by, so that two sets can be held at once.
    TL_TIMELINE_API std::string getOCIOOptionsKey(const OCIOOptions&);

    //! The processors a set of options comes to, before either is a shader.
    //!
    //! The transform is split around the color corrections when the
    //! configuration names a scene linear role, so they operate on linear
    //! values (#328): toLinear is the first half and display the second.
    //! Without the role toLinear is empty and display carries the whole
    //! transform from the input, with the corrections ahead of it.
    struct TL_TIMELINE_API_TYPE OCIOProcessors
    {
        OCIO_NAMESPACE::ConstConfigRcPtr config;
        OCIO_NAMESPACE::DisplayViewTransformRcPtr transform;
        OCIO_NAMESPACE::LegacyViewingPipelineRcPtr lvp;
        OCIO_NAMESPACE::ConstProcessorRcPtr toLinear;
        OCIO_NAMESPACE::ConstProcessorRcPtr display;
    };

    //! Get the processors for a set of options. Throws when the
    //! configuration, or either processor, cannot be had.
    TL_TIMELINE_API OCIOProcessors getOCIOProcessors(const OCIOOptions&);

    //! The processor a LUT file comes to.
    struct TL_TIMELINE_API_TYPE OCIOLUTProcessor
    {
        OCIO_NAMESPACE::ConstConfigRcPtr config;
        OCIO_NAMESPACE::FileTransformRcPtr transform;
        OCIO_NAMESPACE::ConstProcessorRcPtr processor;
    };

    //! Get the processor for a LUT file. Throws when the file cannot be
    //! read as one.
    TL_TIMELINE_API OCIOLUTProcessor getOCIOLUTProcessor(const LUTOptions&);
#endif // TLRENDER_OCIO

    ///@}
}
