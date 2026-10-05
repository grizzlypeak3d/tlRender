# The GPU timeline renderer

`tl::gpu::Render` draws timelines with feather-tk's GPU renderer, on Metal or
Vulkan, as `tl::gl::Render` draws them with OpenGL. What that renderer is, how
it is chosen, its environment variables and how HDR reaches a display are in
feather-tk's `lib/ftk/GPU/README.md`; this is what tlRender adds.

## Building

`TLRENDER_GPU` builds it, and wants feather-tk built with `ftk_GPU`.
`etc/Config/default.cmake` turns both on except with OpenGL ES. A build tree
from before has the option in its cache as OFF:

    cmake -S . -B build -Dftk_GPU=ON -DTLRENDER_GPU=ON

## What is where

- `tlRender/GPU`: `tl::gpu::Render`, a port of `tl::gl::Render`. It draws
  through an `ftk::gpu::Render` and adds its own shaders with `setShader()`.
- `tlRender/Timeline`: what the two timeline renderers do the same way is
  here once. `IRender::_drawBackground()` and `_drawForeground()` draw the
  background and foreground through `ftk::IRender`. `RenderPrivate.h` has
  the OpenColorIO configuration, the processors a set of options or a LUT
  comes to, and the keys they are kept by. What a renderer makes of a
  processor, its shader and textures, is its own.
- `tlRender/UI/Viewport.cpp`: `_drawGPU()`.
- `tlRender/BakeApp`: `tlbake` draws with this renderer when `FTK_RENDER=gpu`
  asks for it, and then has no OpenGL window or context.
- `tlplay` needed nothing: it is feather-tk's windows and the viewport.

## How it differs from the OpenGL renderer

- OpenColorIO writes the shader for each. Its Metal shader takes its
  textures as arguments, so the entry point is written from its signature;
  its Vulkan GLSL declares them itself, in the set and at the bindings asked
  for. OpenColorIO's one dimensional tables are kept as two dimensional
  textures, there being no one dimensional ones.
- A constant array in the GLSL OpenColorIO writes is taken out of it and
  read from a texture by index. The ACES 2 transforms have one, a table of
  363 hues that is searched for each pixel. Left as it was written, Vulkan
  on a Raspberry Pi took ten seconds to compile the shader and up to a
  second to draw with it, where OpenGL ES on the same machine, which keeps
  such an array as a uniform, drew at speed. Metal's is left as it is.
- The wipe cuts the picture's rectangle along the line rather than using the
  stencil buffer.

## HDR pictures

A picture is HDR when it is said to be: `tl::ui::Viewport::setHDRTransfer()`,
SDR or PQ. Nothing in OpenColorIO says a display is PQ, so it is a setting,
for when the OpenColorIO display is an HDR one or the picture is PQ and shown
as it is.

A PQ picture is drawn into the viewport's buffer as its own code values, so
the color picker reads what the file holds, and is taken into what the window
holds as that buffer is drawn there (`tl::gpu::Render::drawTextureHDR()`).
`setHDRWhite()` is the luminance the window's white stands for where the
system does not say: 203 nits, the reference white, by default.

`Viewport::getColorSampleNits()` is the luminance a sample stands for: what a
PQ picture's code values say, or what an HDR window makes of an SDR picture,
and nothing for an SDR picture in an SDR window. `getColorSampleDisplay()` is
the sample as the window shows it.

Not done: HLG. The levels have been looked at on HDR displays and have not
been measured.

## Writing files

With this renderer `tlbake`, and an application's export, draw into an
`ftk::gpu::OffscreenBuffer` and read it back as the writer's own pixel type
with `OffscreenBuffer::read(const ImageInfo&)`: three channels of four, ten
bits packed, rows from the bottom, aligned, and in the byte order asked for.

Against OpenGL, a picture that was RGB to begin with is written as the same
file, byte for byte, in eight, ten and sixteen bits, half and float, with
OpenColorIO and with a LUT. Where the renderers do arithmetic they round
apart by one code value in places: a YUV source, a dissolve, a picture that
is scaled.

Reading back costs a little more than `glReadPixels`: the wait for each
frame to come back before the next is drawn. Without a desktop both
renderers want `SDL_VIDEODRIVER=offscreen`.

## Checking it

`TLRENDER_TESTS` builds two programs, each ending with `PASS` or `FAIL`:

- `tl-gpu-ocio-test` runs OpenColorIO through a pipeline against
  OpenColorIO's own CPU processor: its Metal shader on Metal, and its Vulkan
  GLSL through glslang elsewhere.
- `tl-gpu-hdr-test` draws PQ code values into a window's buffer and presents
  them into an HDR10 swapchain, where they come out as they went in, colors
  outside Rec. 709 included.

`TLRENDER_PROGRAMS` builds `tlbake`, which can be compared with itself:

    tlbake input.mov gl.0.png
    FTK_RENDER=gpu tlbake input.mov gpu.0.png

An application is compared by screenshot, as in feather-tk's notes.

## Known differences from OpenGL

Besides feather-tk's: the clipping warning's outline is a pixel away in
places. It is a threshold, which makes a whole color of a difference too
small to see.
