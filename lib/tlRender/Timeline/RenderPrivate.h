// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/ColorOptions.h>
#include <tlRender/Timeline/Export.h>
#include <tlRender/Timeline/Video.h>

#include <ftk/Core/Box.h>
#include <ftk/Core/Image.h>
#include <ftk/Core/Matrix.h>

#if defined(TLRENDER_OCIO)
#include <OpenColorIO/OpenColorIO.h>
#include <OpenColorIO/OpenColorTransforms.h>
#endif // TLRENDER_OCIO

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tl
{
    //! \name Renderers
    //! What a timeline renderer does that is the same whatever it draws
    //! with: tl::gl::Render and tl::gpu::Render both use these. The drawing
    //! they share is in IRender.
    ///@{

    //! Get a box transformed by a matrix, to whole pixels.
    TL_TIMELINE_API ftk::Box2I xform(const ftk::Box2I&, const ftk::M44F&);

    //! A stage of the color a picture is shown through.
    enum class ColorStage
    {
        //! The LUT file.
        LUT,
        //! The input color space to scene linear: the first half of the
        //! OCIO transform, when the configuration names that role.
        ToLinear,
        //! The color corrections: the color matrix, levels, exposure and
        //! soft clip, which are not OCIO's.
        Corrections,
        //! The display transform: the second half of the OCIO transform,
        //! or all of it.
        Display
    };

    //! Get the color stages in the order they are applied: the corrections
    //! between the transform to scene linear and the display transform, so
    //! that they operate on linear values (#328), and the LUT on whichever
    //! side of the color management it was asked for. Every renderer, and
    //! the thumbnails, goes by this.
    TL_TIMELINE_API std::vector<ColorStage> getColorStages(LUTOrder);

    //! The calls a shader makes for its color stages, either side of the
    //! corrections, in the order getColorStages() gives: one to a line,
    //! indented as given, and those of stages that are not there left out.
    struct TL_TIMELINE_API_TYPE ColorStageCalls
    {
        std::string before;
        std::string after;
    };

    //! Get the calls of a shader's color stages, each the statement that
    //! applies that stage, or empty where there is none.
    TL_TIMELINE_API ColorStageCalls getColorStageCalls(
        LUTOrder,
        const std::string& lut,
        const std::string& toLinear,
        const std::string& display,
        const std::string& indent = "    ");

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

    //! What a renderer builds from the OCIO options -- its processors as
    //! shaders and textures, which T holds -- kept by the options they were
    //! built for and then by the input color space, so that layers with
    //! different inputs each draw through their own transform, and so that
    //! more than one set of options is held at once. The viewport and the
    //! timeline items draw through the same renderer with different
    //! options, so the two sets alternate within every frame: keeping only
    //! the current one meant reading the configuration and recompiling the
    //! shaders twice a frame. The last few sets are kept.
    //!
    //! Also the layers' input color spaces, found from where each came
    //! from by a resolver and kept by the options and the path.
    template<typename T>
    class OCIOCache
    {
    public:
        OCIOCache();

        //! Get the current options, and their key.
        const OCIOOptions& getOptions() const;
        const std::string& getKey() const;

        //! Switch to a set of options, keeping what was built for the last
        //! few. Returns the keys of the sets let go of, for what the
        //! renderer built from them, its shaders, to go as well.
        std::vector<std::string> setOptions(const OCIOOptions&);

        //! Get what is built for an input color space under the current
        //! options: built by init the first time. One that cannot be built
        //! -- init throws -- is remembered as null, so the picture draws
        //! without color management rather than not at all, and is not
        //! tried again every frame; error is told why, once.
        std::shared_ptr<T> get(
            const std::string& input,
            const std::function<void(T&, const OCIOOptions&)>& init,
            const std::function<void(const std::string&)>& error);

        //! Set what finds a layer's input color space from where it came
        //! from. What it found before is forgotten.
        void setInputResolver(
            const std::function<std::string(
                const std::string& path,
                const ftk::ImageTags&)>&);

        //! Get a layer's input color space: its own when it has one, or
        //! what the resolver makes of its path and its image's tags.
        std::string getLayerInput(
            const std::string& layerInput,
            const std::string& path,
            const std::shared_ptr<ftk::Image>&);

        //! Get the input color spaces of a frame's layers, of the A image
        //! and the B image of each, the item's where a layer has none.
        //! Empty when there is no color management to draw through.
        std::vector<std::pair<std::string, std::string> > getLayerInputs(
            const VideoFrame&,
            const std::string& itemInput);

    private:
        static const size_t _keysMax = 4;

        OCIOOptions _options;
        std::string _key;
        std::list<std::string> _keys;
        std::map<std::string, std::shared_ptr<T> > _data;
        std::function<std::string(const std::string&, const ftk::ImageTags&)> _resolver;
        std::map<std::string, std::string> _inputs;
    };

    //! Get the first input color space of a frame's layers, for what
    //! applies to them all: whether the configuration splits the transform
    //! at scene linear.
    TL_TIMELINE_API std::string getFirstLayerInput(
        const std::vector<std::pair<std::string, std::string> >&);
#endif // TLRENDER_OCIO

    ///@}
}

#include <tlRender/Timeline/RenderPrivateInline.h>
