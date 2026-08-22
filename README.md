[![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Build Status](https://github.com/grizzlypeak3d/tlRender/actions/workflows/ci-workflow.yml/badge.svg)](https://github.com/grizzlypeak3d/tlRender/actions/workflows/ci-workflow.yml)

# ![tlRender Icon](etc/Icons/tlRender_32.png)&nbsp;tlRender

tlRender is an open source library for building playback and review
applications for visual effects, film, and animation.

The library can render and playback timelines with multiple video clips,
image sequences, audio clips, and transitions. Examples are provided for
integrating the library with OpenGL applications.

The library is written in C++ and uses the CMake build system.

These screenshots show an example application built with tlRender. The
application is comparing two images with a wipe and horizontal layout.

![player 1](etc/Images/player_1.png)
![player 2](etc/Images/player_2.png)

Features:
* Support for timelines, image sequences, movies, and audio files
* A/B comparison with wipe, overlay, and difference modes
* Color management with OpenColorIO
* Multi-track audio with variable speed and reverse playback
* Available for Linux, macOS, and Windows


## Building Dependencies

A CMake super build script is provided to build all of the dependencies from
source.

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


## Building on Linux

Requirements:
* Git
* CMake 3.31

#### Debian

Install system packages:
```
sudo apt-get install build-essential git cmake xorg-dev libglu1-mesa-dev mesa-common-dev mesa-utils libasound2-dev libpulse-dev libva-dev libdrm-dev
```

#### Rocky 9

Install system packages:
```
sudo dnf install git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel pipewire-devel libva-devel libdrm-devel
```

#### Rocky 8

Install system packages:
```
sudo dnf install git libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel mesa-libGL-devel pipewire-devel libva-devel libdrm-devel
```
Install newer compiler:
```
sudo dnf install gcc-toolset-13
```
Enable newer compiler:
```
scl enable gcc-toolset-13 bash
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

Try running the `tlplay` application:
```
./build-Release/bin/tlplay/tlplay tlRender/etc/SampleData/MultipleClips.otio
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

Try running the `tlplay` application:
```
./build-Release/bin/tlplay/tlplay tlRender/etc/SampleData/MultipleClips.otio
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

Try running the `tlplay` application:
```
set PATH=%CD%\install-Release\bin;%PATH%
```
```
.\build-Release\bin\tlplay\Release\tlplay tlRender\etc\SampleData\MultipleClips.otio
```
