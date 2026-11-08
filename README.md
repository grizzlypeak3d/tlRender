[![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Build Status](https://github.com/grizzlypeak3d/tlRender/actions/workflows/ci-workflow.yml/badge.svg)](https://github.com/grizzlypeak3d/tlRender/actions/workflows/ci-workflow.yml)
[![PyPI](https://img.shields.io/pypi/v/tlrender.svg)](https://pypi.org/project/tlrender/)

# ![tlRender Icon](https://raw.githubusercontent.com/grizzlypeak3d/tlRender/main/etc/Icons/tlRender_32.png)&nbsp;tlRender

tlRender is an open source library for building playback and review
applications for visual effects, film, and animation.

The library can render and playback timelines with multiple video clips,
image sequences, audio clips, and transitions. Examples are provided for
integrating the library with OpenGL applications.

The library is written in C++ and uses the CMake build system, with Python
bindings on PyPI:

```sh
pip install tlRender
```

These screenshots show an example application built with tlRender. The
application is comparing two images with a wipe and horizontal layout.

![player 1](https://raw.githubusercontent.com/grizzlypeak3d/tlRender/main/etc/Images/player_1.png)
![player 2](https://raw.githubusercontent.com/grizzlypeak3d/tlRender/main/etc/Images/player_2.png)

Features:
* Support for timelines, image sequences, movies, and audio files
* A/B comparison with wipe, overlay, and difference modes
* Color management with OpenColorIO
* Multi-track audio with variable speed and reverse playback
* Experimental support for USD files
* Available for Linux, macOS, and Windows


## Quick start

A window that plays a timeline, movie, or image sequence given on the
command line.

### C++

```cpp
#include <tlRender/UI/Init.h>
#include <tlRender/UI/Viewport.h>
#include <tlRender/Timeline/Player.h>

#include <ftk/UI/App.h>
#include <ftk/UI/MainWindow.h>

#include <iostream>

using namespace ftk;

int main(int argc, char** argv)
{
    try
    {
        // Create the context and application.
        auto context = Context::create();
        tl::ui::init(context);
        auto input = CmdLineArg<std::string>::create(
            "input", "A timeline, movie, or image sequence.");
        auto app = App::create(
            context, argc, argv, "simple", "Simple player example.", { input });
        if (app->hasCmdLineHelp())
            return 0;

        // Create a timeline and a player for it.
        auto timeline = tl::Timeline::create(context, Path(input->getValue()));
        auto player = tl::Player::create(context, timeline);

        // Show the player in a window.
        auto window = MainWindow::create(context, app);
        auto viewport = tl::ui::Viewport::create(context);
        viewport->setPlayer(player);
        window->setWidget(viewport);

        // Start playback and run the application.
        player->setPlayback(tl::Playback::Forward);
        app->run();
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### Python

The same player, after `pip install tlRender`:

```python
import feather_tk as ftk
import tlrender as tl
import sys

# Create the context and application.
context = ftk.Context()
tl.ui.init(context)
input = ftk.CmdLineArgString("input", "A timeline, movie, or image sequence.")
app = ftk.App(context, sys.argv, "simple", "Simple player example.", [input])
if app.hasCmdLineHelp:
    sys.exit(0)

# Create a timeline and a player for it.
timeline = tl.Timeline(context, ftk.Path(input.value))
player = tl.Player(context, timeline)

# Show the player in a window.
window = ftk.MainWindow(context, app)
viewport = tl.ui.Viewport(context)
viewport.player = player
window.widget = viewport

# Start playback and run the application.
player.playback = tl.Playback.Forward
app.run()
```

The [examples](https://github.com/grizzlypeak3d/tlRender/tree/main/examples)
directory has fuller players in both languages, with playback controls and a
timeline.


## Web Player

An experimental build of the example player runs in the browser,
playing movies with the WebCodecs API:

* [Test pattern](https://grizzlypeak3d.github.io/tlRender/player/)
* [ASC StEM2](https://grizzlypeak3d.github.io/tlRender/player/?url=https://aswf-dpel-assets.s3.amazonaws.com/asc-stem2/ASC_StEM2_178_2K_24_100nits_Rec709_Stereo.mp4)
  from the [ASWF Digital Production Example Library](https://dpel.aswf.io)

The "url" query plays a movie from any host that allows cross-origin
range reads. The page asks the browser for as much memory as it will
give, from 2GB down; a "mem" query names the maximum in megabytes for
a browser that refuses even the smallest, or for trying one. Decoded frames stay on the GPU: the browser imports each
one into a WebGL texture and the viewport draws it there, with no copy
through memory. The "copy" query takes the older path, which copies
every frame into memory, for comparison.

The web build is `etc/Web/build.sh`, which builds the dependencies and
the player with Emscripten; it needs `emcc` on the path. The Web
workflow runs it on every push to main and publishes the result to the
page above.


## Python

The Python bindings are one wheel per platform for CPython 3.12 and later,
built on the [feather-tk](https://pypi.org/project/feather-tk/) wheel, which
pip installs with them, and
[OpenTimelineIO](https://pypi.org/project/opentimelineio/). Python 3.12 or
3.13 is the easy choice for now: OpenTimelineIO has no prebuilt wheels yet
for Python 3.14, or for Intel macOS, and there pip builds it from source,
which needs a C++ compiler and the Python development files.

```python
import opentimelineio as otio
import feather_tk as ftk
import tlrender as tl
```

The wheel has only the FFmpeg codecs that need no patent license: AV1, APV,
VP8 and VP9, MPEG-2 and MPEG-4, MJPEG, FFV1, CineForm, UT Video, HuffYUV,
MagicYUV, Dirac, PNG, v210, and the FLAC, Opus, Vorbis, ALAC, MP3 and PCM
audio codecs. **H.264, HEVC, ProRes, DNxHD, DV and AAC are not in it**, so a
camera movie or anything else carrying them does not open with the wheel
alone. Two ways to play them: install FFmpeg yourself, and tlRender reads
them with the `ffmpeg` and `ffprobe` commands on `PATH`; or build from
source, where the codecs are compiled in.

OpenTimelineIO does not publish wheels for every platform tlRender does, so
on some pip builds it from source, which needs a C++ compiler.

### Without a display

A script with no window asks the timeline for frames. `Timeline.getVideo()`
returns once the frame is decoded, with no player, clock or audio device.
One frame of a sequence opens that frame alone; `ftk.expandSeq` finds the
rest of the sequence beside it on disk:

```python
import numpy as np
import feather_tk as ftk
import tlrender as tl

context = ftk.Context()
tl.init(context)
path = ftk.expandSeq(ftk.Path("render.0001.exr"), ftk.PathOptions())
timeline = tl.Timeline(context, path)

frame = timeline.getVideo(timeline.timeRange.start_time)
image = frame.layers[0].image
pixels = np.asarray(image)       # (height, width, channels), the right way up
print(pixels.dtype, pixels.shape, pixels.max())
```

A time with no clip gives a frame with no layers, and a frame that cannot be
read a layer whose image is `None`.

A `Timeline` also takes an `opentimelineio.schema.Timeline` object, so a
timeline built or edited in Python plays without being written to a file
first.

`OffscreenRender` draws frames the way a viewport does, a compare included,
to an image you can write or read. A compare is a frame from each timeline,
the second at the time `getCompareTime()` gives, counted from its own start
as a `Player` compares them:

```python
v002 = tl.Timeline(context,
    ftk.expandSeq(ftk.Path("v002/render.0001.exr"), ftk.PathOptions()))
options = tl.CompareOptions()
options.compare = tl.Compare.Tile
t = timeline.timeRange.start_time
frames = [timeline.getVideo(t),
          v002.getVideo(tl.getCompareTime(t, timeline.timeRange, v002.timeRange))]
image = tl.OffscreenRender(context).render(frames, options)

info = tl.IOInfo()
info.video = [image.info]
writer = tl.WriteSystem(context).write(ftk.Path("compare.png"), info)
writer.writeVideo(t, image)
writer.finish()
```

Keep one `OffscreenRender` and render the same frames again with other
options -- a wipe position, a size, a look -- and the frames are kept on the
GPU rather than copied in each time. They are told apart by the image, so
after changing an image's pixels in place call `clearCache()`.
[compare.py](https://github.com/grizzlypeak3d/tlRender/tree/main/examples/python/compare.py)
renders a tile and then sweeps a wipe this way.

Its OpenGL context is a hidden window's. On Linux with no display at all --
a container, a render farm node, continuous integration -- set
`SDL_VIDEO_DRIVER=offscreen` in the environment, and it is an EGL context
with no window system. That needs Mesa's EGL and DRI drivers (on Debian and
Ubuntu, `libegl1 libgl1-mesa-dri libgbm1`), and uses the GPU where there is
one and Mesa's software renderer where there is not. Where there is no sound
card either, set `SDL_AUDIO_DRIVER=dummy`: audio then goes nowhere, and the
audio libraries are not asked for devices, which is what prints their errors
in a container.

For playback, a `Player` decodes ahead of its current time on threads of its
own, and what it has decoded reaches `currentVideo` only in `Context.tick()`.
A tick does not wait for anything, so a script playing without an
application's event loop sleeps a few milliseconds between ticks. After a
seek `currentVideo` holds the previous frame until the new one is decoded;
check each frame's time.

The [Python examples](https://github.com/grizzlypeak3d/tlRender/tree/main/examples/python)
show the API in use. The wheel also carries the C++ libraries, headers, and
CMake package, so a project with its own bindings can build against the same
installation:

```sh
cmake -DtlRender_DIR="$(python -c "import tlrender; print(tlrender.get_cmake_dir())")" ...
```

tlRender's package finds feather-tk's in the `feather_tk` package beside it.


## Building Dependencies

A CMake super build script is provided to build all of the dependencies from
source.

Build options go in `etc/Config/local.cmake`, which is not tracked. For
example, to enable USD (building USD also requires Python 3):

```cmake
set(TLRENDER_USD ON CACHE BOOL "")
```

Required dependencies:
* [feather-tk](https://github.com/grizzlypeak3d/feather-tk)
* [Imath](https://github.com/AcademySoftwareFoundation/Imath)
* [minizip-ng](https://github.com/zlib-ng/minizip-ng)
* [OpenTimelineIO](https://github.com/PixarAnimationStudios/OpenTimelineIO)

Optional dependencies:
* [OpenSSL](https://www.openssl.org)
* [libssh2](https://libssh2.org)
* [curl](https://curl.se/libcurl)
* [OpenColorIO](https://github.com/AcademySoftwareFoundation/OpenColorIO)
* [SDL2](https://www.libsdl.org)
* [JPEG](https://libjpeg-turbo.org)
* [TIFF](http://www.libtiff.org)
* [PNG](https://libpng.sourceforge.io/index.html)
* [OpenEXR](https://www.openexr.com/)
* [FFmpeg](https://ffmpeg.org)
* [OpenUSD](https://github.com/PixarAnimationStudios/OpenUSD)


## Building on Linux

Requirements:
* Git
* CMake 3.31

#### Debian

Install system packages:
```
sudo apt-get install build-essential git cmake xorg-dev libglu1-mesa-dev mesa-common-dev mesa-utils libasound2-dev libpulse-dev libpipewire-0.3-dev libva-dev libdrm-dev libwayland-dev wayland-protocols libxkbcommon-dev libegl1-mesa-dev libdecor-0-dev
```
The Wayland packages are optional; without them SDL is built with X11 only and runs through XWayland on a Wayland desktop.

#### Rocky 9

Install system packages:
```
sudo dnf install git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel pipewire-devel libva-devel libdrm-devel wayland-devel wayland-protocols-devel libxkbcommon-devel mesa-libEGL-devel libdecor-devel
```

#### Rocky 8

Install system packages:
```
sudo dnf install git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel pipewire-devel libva-devel libdrm-devel wayland-devel wayland-protocols-devel libxkbcommon-devel mesa-libEGL-devel
```
Install newer compiler:
```
sudo dnf install gcc-toolset-13
```
Enable newer compiler:
```
scl enable gcc-toolset-13 bash
```

NVIDIA hardware decoding loads the driver's CUDA library at run time. On
RHEL family systems that is a separate package:
```
sudo dnf install nvidia-driver-cuda-libs
```

#### Build

Clone the repository:
```
git clone https://github.com/grizzlypeak3d/tlRender.git
```

Run the super build script:
```
sh tlRender/sbuild-linux.sh
```

Try running the example player:
```
./build-Release/examples/player/player -url tlRender/etc/SampleData/MultipleClips.otio
```

Example running gcovr for code coverage:
```
gcovr -r ../../../../lib --html --object-directory lib --html-details --output gcov.html lib/tlCore lib/tlIO lib/tlTimeline
```


## Building on macOS

Requirements:
* Git
* Xcode
* CMake 3.31

Clone the repository:
```
git clone https://github.com/grizzlypeak3d/tlRender.git
```

Run the super build script:
```
sh tlRender/sbuild-macos.sh
```

Try running the example player:
```
./build-Release/examples/player/player -url tlRender/etc/SampleData/MultipleClips.otio
```

These aliases are convenient for switching between architectures:
```
alias arm="env /usr/bin/arch -arm64 /bin/zsh --login"
alias intel="env /usr/bin/arch -x86_64 /bin/zsh --login"
```


## Building on Windows

Requirements:
* Git (https://git-scm.com)
* Visual Studio 2022
* CMake 3.31
* NASM (https://www.nasm.us) for compiling FFmpeg and libjpeg-turbo.
* MSYS2 (https://www.msys2.org) for compiling FFmpeg.
* Strawberry Perl (https://strawberryperl.com/) for compiling network support.
* Python 3.11 for compiling USD.

Open the Visual Studio command console "x64 Native Tools Command Prompt for VS 2022".
This can be found in the Start menu, in the "Visual Studio 2022" folder.

Clone the repository:
```
git clone https://github.com/grizzlypeak3d/tlRender.git
```

Run the super build script:
```
tlRender\sbuild-win.bat
```

Try running the example player:
```
set PATH=%CD%\install-Release\bin;%PATH%
```
```
.\build-Release\examples\player\Release\player -url tlRender\etc\SampleData\MultipleClips.otio
```
