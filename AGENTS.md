# tlRender for agents

Instructions for a coding agent working on tlRender or using it from a
script. People are welcome to read it too.

tlRender is a C++ (CMake, C++20) library for playing back timelines, image
sequences and movies, with OpenTimelineIO. It is the engine under DJV. The
user interface toolkit is feather-tk, a submodule in `deps/ftk`.

## Where things are

- `lib/tlRender/Core`: time, image and audio types, OCIO, the file system.
- `lib/tlRender/IO`: readers and writers, one plugin per format (FFmpeg,
  OpenEXR, PNG, JPEG, TIFF, OpenImageIO, ...).
- `lib/tlRender/Timeline`: timelines and players: what is shown at a time,
  caching, audio.
- `lib/tlRender/GL`, `GPU`: drawing, including `OffscreenRender` for
  rendering without a window.
- `lib/tlRender/UI`: the timeline and viewport widgets.
- `lib/tlRender/C`: a C interface.
- `lib/tlRender/*Py`, `tlRenderPy`: the Python bindings (nanobind), the
  `tlrender` module.
- `tests`: `tl-test` (C++), `tl-c-test` (C), `tlRenderPyTest` (Python).
- `etc/SuperBuild`: how each dependency is built, with its version.

## Building and testing

The super build builds the dependencies; tlRender is then built against
them. The platform sections of README.md give the commands. Turn the tests
on with `-DTLRENDER_TESTS=ON`, and Python with `-DTLRENDER_PYTHON=ON`
(Python 3.12 or newer). Then, in the build directory:

```
ctest -j6
```

`tl-test` takes a filter, matched as a substring: `tl-test FFmpegTest`.
`tl-test -noGL` skips the tests that need OpenGL. A test build that was not
rebuilt runs old binaries, so check that the test executables are newer
than your change.

Memory errors here can pass on one platform and crash on another. A
sanitizer build finds them: `-DTLRENDER_SANITIZE=address,undefined` in a
Debug build, run with `ASAN_OPTIONS=detect_container_overflow=0` (the
container check gives false reports where a dependency was built without
the sanitizer).

## Using tlRender from Python

Install the wheel (`pip install tlRender`), or build with Python on. Open a
timeline or a file and ask it for frames; no window, player or audio device
is needed:

```python
import numpy as np
import feather_tk as ftk
import tlrender as tl

context = ftk.Context()
tl.init(context)
path = ftk.expandSeq(ftk.Path("render.0001.exr"), ftk.PathOptions())
timeline = tl.Timeline(context, path)
frame = timeline.getVideo(timeline.timeRange.start_time)
pixels = np.asarray(frame.layers[0].image)   # (height, width, channels)
```

`tl.OffscreenRender` draws frames the way the viewport does, comparisons
included, into an image you can write to a file; it works with no display
(`SDL_VIDEO_DRIVER=offscreen` on Linux). README.md, "Python", has more,
including playback with a `tl.Player`.

## Changing tlRender

- A new format is an IO plugin: a read and a write plugin, added to the
  lists in `lib/tlRender/IO/System.cpp`, with a test in `lib/tlRender/IOTest` that
  writes a file and reads it back.
- Readers run on their own threads and are asked for frames out of order;
  a reader must give the same frame for the same time, whatever was asked
  for before.
- A reader may store an image bottom up; `layout.mirror.y` in its
  information says so, and code that reads pixels has to check it.
- A dependency's version is in its `etc/SuperBuild/cmake/Modules/Build*.cmake`.

## Contributions

- Tests with every change, and the existing tests passing.
- One change per pull request.
- No new dependencies without discussing them first.
- The person who sends a change stands behind it, however it was written.
