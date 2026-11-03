// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

//! \file Engine.h
//! A C interface to the engine: the player and a view that draws it,
//! for Swift and any other language that speaks C. Everything here is
//! called on the main thread.

#include <stddef.h>
#include <tlRender/Timeline/Export.h>

#ifdef __cplusplus
extern "C" {
#endif

//! The engine: a context, the systems, and the GPU renderer's device.
typedef struct tl_engine tl_engine;

//! A timeline player.
typedef struct tl_player tl_player;

//! A view that draws a player into a native view.
typedef struct tl_view tl_view;

typedef enum
{
    TL_PLAYBACK_STOP,
    TL_PLAYBACK_FORWARD,
    TL_PLAYBACK_REVERSE
} tl_playback;

//! Create the engine. NULL when the GPU renderer cannot start; the
//! reason is in tl_engine_get_error() of a NULL-free call, or the log.
TL_TIMELINE_API tl_engine* tl_engine_create(void);
TL_TIMELINE_API void tl_engine_destroy(tl_engine*);

//! Tick the engine: once per display frame, before the views draw.
TL_TIMELINE_API void tl_engine_tick(tl_engine*);

//! Set an I/O option for the files opened from now on (see IO.h): for
//! instance "Texture" "0" to copy decoded frames rather than draw them
//! where the decoder put them.
TL_TIMELINE_API void tl_engine_set_io_option(tl_engine*, const char* name, const char* value);

//! Override the video cache size and the number of readers kept open
//! for the files opened from now on; zero leaves the platform's choice.
TL_TIMELINE_API void tl_engine_set_cache(tl_engine*, float videoGB, int readCacheMax);

//! The last error, or an empty string.
TL_TIMELINE_API const char* tl_engine_get_error(tl_engine*);

//! Open a file or timeline. NULL on failure, with the reason in the
//! engine's error.
TL_TIMELINE_API tl_player* tl_player_create(tl_engine*, const char* path);
TL_TIMELINE_API void tl_player_destroy(tl_player*);

//! Times are seconds from the start of the timeline.
TL_TIMELINE_API double tl_player_get_duration(tl_player*);
TL_TIMELINE_API double tl_player_get_rate(tl_player*);
TL_TIMELINE_API double tl_player_get_time(tl_player*);
//! The time of the video frame being shown, which trails the current
//! time while the reader catches up; negative when there is none yet.
TL_TIMELINE_API double tl_player_get_video_time(tl_player*);
TL_TIMELINE_API void tl_player_seek(tl_player*, double seconds);
TL_TIMELINE_API void tl_player_set_playback(tl_player*, tl_playback);
TL_TIMELINE_API tl_playback tl_player_get_playback(tl_player*);

//! What happens at the end of the in/out range.
typedef enum
{
    TL_LOOP_LOOP,
    TL_LOOP_ONCE,
    TL_LOOP_PING_PONG
} tl_loop;
TL_TIMELINE_API void tl_player_set_loop(tl_player*, tl_loop);
TL_TIMELINE_API tl_loop tl_player_get_loop(tl_player*);

//! The playback speed in frames a second; the timeline's rate to begin
//! with.
TL_TIMELINE_API double tl_player_get_speed(tl_player*);
TL_TIMELINE_API void tl_player_set_speed(tl_player*, double);

//! Moves of the current time: to the ends, by frames, by seconds.
typedef enum
{
    TL_TIME_START,
    TL_TIME_END,
    TL_TIME_FRAME_PREV,
    TL_TIME_FRAME_PREV_X10,
    TL_TIME_FRAME_PREV_X100,
    TL_TIME_FRAME_NEXT,
    TL_TIME_FRAME_NEXT_X10,
    TL_TIME_FRAME_NEXT_X100,
    TL_TIME_JUMP_BACK_1S,
    TL_TIME_JUMP_BACK_10S,
    TL_TIME_JUMP_FORWARD_1S,
    TL_TIME_JUMP_FORWARD_10S
} tl_time_action;
TL_TIMELINE_API void tl_player_time_action(tl_player*, tl_time_action);

//! The in/out range: set from the current time, reset to the timeline's
//! ends, and read as seconds from the start of the timeline.
TL_TIMELINE_API void tl_player_set_in_point(tl_player*);
TL_TIMELINE_API void tl_player_reset_in_point(tl_player*);
TL_TIMELINE_API void tl_player_set_out_point(tl_player*);
TL_TIMELINE_API void tl_player_reset_out_point(tl_player*);
TL_TIMELINE_API void tl_player_get_in_out(tl_player*, double* start, double* end);
//! Set the in/out range, in seconds from the start of the timeline, the
//! end exclusive.
TL_TIMELINE_API void tl_player_set_in_out(tl_player*, double start, double end);
TL_TIMELINE_API int tl_player_get_video_width(tl_player*);
TL_TIMELINE_API int tl_player_get_video_height(tl_player*);

//! A clip on one of the timeline's video tracks: which track, counting
//! from the bottom, where it is in seconds from the start of the
//! timeline, and its name (the media's file name when it has none). A
//! nested stack on a track is listed as a clip too.
typedef struct tl_clip
{
    int track;
    double start;
    double duration;
    char name[256];
    //! The color the editor gave the clip, RGBA, if any: hasColor says.
    float color[4];
    int hasColor;
} tl_clip;

//! The clips of the video tracks, track by track, each in time order.
TL_TIMELINE_API int tl_player_get_video_track_count(tl_player*);
TL_TIMELINE_API int tl_player_get_clip_count(tl_player*);
TL_TIMELINE_API int tl_player_get_clip(tl_player*, int index, tl_clip*);

//! A shot as seen: a stretch of the timeline where the picture comes
//! from one clip, the one on the highest video track there, as OTIO's
//! flatten_stack() finds it. Which clip it is, as an index in the clip
//! list (a clip that a higher track's clip cuts into is more than one
//! shot), its clip's track and name, and where it is in seconds from
//! the start of the timeline.
typedef struct tl_shot
{
    int clip;
    int track;
    double start;
    double duration;
    char name[256];
    //! The clip's color, as tl_clip has it.
    float color[4];
    int hasColor;
} tl_shot;

//! The shots, in time order. Where no track has a clip, none.
TL_TIMELINE_API int tl_player_get_shot_count(tl_player*);
TL_TIMELINE_API int tl_player_get_shot(tl_player*, int index, tl_shot*);

//! A marker the editor left: on the timeline, a track, or a clip, in
//! seconds from the start of the timeline. A duration of zero marks a
//! point. The color is OTIO's, green when the marker has none.
typedef struct tl_marker
{
    double start;
    double duration;
    float color[4];
    char name[256];
    char comment[1024];
} tl_marker;

//! The markers, in time order: the timeline's, and those of the enabled
//! tracks and their clips.
TL_TIMELINE_API int tl_player_get_marker_count(tl_player*);
TL_TIMELINE_API int tl_player_get_marker(tl_player*, int index, tl_marker*);

//! A picture in memory: RGBA, eight bits a channel, the top row first.
typedef struct tl_image
{
    int width;
    int height;
    unsigned char* data;
} tl_image;
TL_TIMELINE_API void tl_image_free(tl_image*);

//! Ask for a thumbnail of the timeline at a time, in seconds from its
//! start, so many pixels high. Made in the background: zero when it
//! cannot be asked for.
TL_TIMELINE_API unsigned long long tl_player_request_thumbnail(
    tl_player*, double seconds, int height);
//! Whether a thumbnail is ready: one with the picture in the image,
//! which the caller frees; zero while it is being made; minus one when
//! there is none, and the request is done with.
TL_TIMELINE_API int tl_player_get_thumbnail(
    tl_player*, unsigned long long id, tl_image*);
TL_TIMELINE_API void tl_player_cancel_thumbnail(tl_player*, unsigned long long id);

//! A cut opened for what is in it alone: no player, no cache. Unlike
//! the rest of this interface it can be used from any thread, one cut
//! at a time on each, so that a library can read its files in the
//! background. The file stays open, and a bundle mapped, until it is
//! closed: close it soon. NULL when it cannot be read. An .otiod bundle
//! is opened by its folder, here and in tl_player_create().
typedef struct tl_cut tl_cut;
TL_TIMELINE_API tl_cut* tl_cut_open(tl_engine*, const char* path);
TL_TIMELINE_API void tl_cut_close(tl_cut*);
TL_TIMELINE_API double tl_cut_get_duration(tl_cut*);
TL_TIMELINE_API double tl_cut_get_rate(tl_cut*);
TL_TIMELINE_API int tl_cut_get_video_width(tl_cut*);
TL_TIMELINE_API int tl_cut_get_video_height(tl_cut*);
TL_TIMELINE_API int tl_cut_get_clip_count(tl_cut*);
TL_TIMELINE_API int tl_cut_get_clip(tl_cut*, int index, tl_clip*);
TL_TIMELINE_API int tl_cut_get_shot_count(tl_cut*);
TL_TIMELINE_API int tl_cut_get_shot(tl_cut*, int index, tl_shot*);

//! Ask for a poster of a cut: a thumbnail at a time, in seconds from its
//! start, so many pixels high. Made in the background, and the file is
//! not kept open for it once it is made. Collected from the engine, from
//! any thread, as tl_player_get_thumbnail() says; the cut can be closed
//! before it is ready.
TL_TIMELINE_API unsigned long long tl_cut_request_poster(
    tl_cut*, double seconds, int height);
TL_TIMELINE_API int tl_engine_get_poster(
    tl_engine*, unsigned long long id, tl_image*);

//! Ask for the timeline's audio over a range, in seconds from its start,
//! as so many columns: each the lowest and the highest sample under it,
//! of every audio track and channel together, as the timeline widget
//! draws its waveforms. Read in the background, one at a time, through
//! the player's timeline: zero when it cannot be asked for.
TL_TIMELINE_API unsigned long long tl_player_request_waveform(
    tl_player*, double start, double duration, int columns);
//! Whether a waveform is ready: one with the columns copied out, zero
//! while it is being read, minus one when there is none. The columns are
//! pairs, lowest then highest, two floats a column: count is the number
//! of columns, and columns has room for twice as many floats. Full scale
//! is minus one to one; a sample over full scale is not clipped. A column
//! with no sample under it is zero.
TL_TIMELINE_API int tl_player_get_waveform(
    tl_player*, unsigned long long id, float* columns, int count);
TL_TIMELINE_API void tl_player_cancel_waveform(tl_player*, unsigned long long id);

//! How playback is going, for a readout: whether frames are being
//! dropped, and whether the cache is keeping ahead of the playhead.
typedef struct tl_player_stats
{
    //! The speed asked for, and the speed measured, in frames a second.
    double speed;
    double actualSpeed;

    //! Frames dropped since playback started or the last seek.
    size_t droppedFrames;

    //! Whether the clock is held while the cache fills (the pre-roll).
    int buffering;

    //! Seconds of video cached ahead of the current time: zero when
    //! playback is waiting on the reader.
    double cacheAhead;

    //! The caches: how full, and how big, in gigabytes.
    float videoCachePercent;
    float videoCacheGB;
    float audioCachePercent;
    float audioCacheGB;
} tl_player_stats;
TL_TIMELINE_API tl_player_stats tl_player_get_stats(tl_player*);

//! Create a view in a native view: on macOS an NSWindow and the NSView
//! in it, both unretained. NULL on failure, with the reason in the
//! engine's error.
TL_TIMELINE_API tl_view* tl_view_create(tl_engine*, void* nsWindow, void* nsView);

//! Create a view on iOS: a window of its own in the UIWindowScene,
//! which the application keeps under its own window and draws its
//! controls over; tl_view_set_frame() says where the picture goes.
//! The window is tl_view_get_native_window().
TL_TIMELINE_API tl_view* tl_view_create_ios(tl_engine*, void* uiWindowScene);
TL_TIMELINE_API void tl_view_destroy(tl_view*);

//! The native window the view draws into: the UIWindow on iOS, the
//! NSWindow on macOS.
TL_TIMELINE_API void* tl_view_get_native_window(tl_view*);

//! Set the player the view draws; NULL clears it.
TL_TIMELINE_API void tl_view_set_player(tl_view*, tl_player*);

//! Set where in the view the picture is drawn, in pixels from the top
//! left; a zero size means the whole view, which is the default.
TL_TIMELINE_API void tl_view_set_frame(tl_view*, int x, int y, int w, int h);

//! Where the picture is within its frame and how big: the position of
//! its top left corner in view pixels, and the zoom, one being a picture
//! pixel per view pixel. Fitted to the frame until set.
TL_TIMELINE_API void tl_view_get_view(tl_view*, float* x, float* y, float* zoom);
TL_TIMELINE_API void tl_view_set_view(tl_view*, float x, float y, float zoom);
//! Fit the picture to the frame again, and keep it fitted as the frame
//! changes.
TL_TIMELINE_API void tl_view_frame_view(tl_view*);
//! Zoom by a factor about a point in view pixels, which stays where it is.
TL_TIMELINE_API void tl_view_zoom(tl_view*, float factor, float x, float y);
//! Move the picture by a distance in view pixels.
TL_TIMELINE_API void tl_view_pan(tl_view*, float dx, float dy);

//! Where the timeline widget goes in the view, in pixels: an empty frame
//! hides it. The display scale is the screen's, which sizes its text
//! and lines.
TL_TIMELINE_API void tl_view_set_timeline_frame(
    tl_view*, int x, int y, int w, int h, float displayScale);

//! Minimize the timeline widget: the ruler and the tracks as lines,
//! without their thumbnails and labels, as DJV's Minimize does.
TL_TIMELINE_API void tl_view_set_timeline_minimized(tl_view*, int);
//! The size the timeline widget asks for, in pixels, which is the height
//! to give it when minimized. Zero until it has been ticked.
TL_TIMELINE_API void tl_view_get_timeline_size_hint(tl_view*, int* w, int* h);

//! A touch, or a mouse button, on the view: where it is in pixels, and
//! what it is doing. The widgets take it as the mouse.
typedef enum tl_touch_phase
{
    TL_TOUCH_BEGAN,
    TL_TOUCH_MOVED,
    TL_TOUCH_ENDED,
    TL_TOUCH_CANCELLED
} tl_touch_phase;
TL_TIMELINE_API void tl_view_touch(tl_view*, tl_touch_phase, int x, int y);

//! Two fingers on the view: the point between them in pixels, how far
//! it moved since the last call, and how much their distance changed as
//! a ratio. The widgets under them take it: the timeline scrolls by the
//! pan and zooms by the pinch. The first finger's touch is cancelled by
//! the widgets when the gesture begins.
TL_TIMELINE_API void tl_view_gesture(
    tl_view*, tl_touch_phase, int x, int y, float panX, float panY, float zoom);

//! Draw the player's current frame, fitted to the view, and present it.
//! Whether the view has something new to draw: a video frame that has
//! not been drawn, a widget that asked for a redraw, a change of size or
//! frame. Ticks the widgets, so it is asked every tick, and the draw is
//! made only when it says so: drawing at the display's rate when the
//! picture changes at the movie's wasted most of the draws.
TL_TIMELINE_API int tl_view_needs_draw(tl_view*);
//! How many times the view has been drawn, for a readout of draws a second.
TL_TIMELINE_API size_t tl_view_get_draw_count(tl_view*);
TL_TIMELINE_API void tl_view_draw(tl_view*);

//! How far writing a bundle has got: minus one now and then while the
//! media folder is being looked through, then the fraction of the media's
//! bytes copied, zero to one, when the copying starts and after each
//! piece of it. Called on the writing thread. Return zero to stop.
typedef int (*tl_bundle_progress)(double fraction, void* user);

//! What writing a bundle did.
typedef struct tl_bundle_result
{
    //! The media files the cut uses, an image sequence counting as one.
    int media;
    //! How many of them were not found. Their references are left as
    //! they were.
    int missing;
    //! The names of the first ones not found, a line each.
    char missingNames[1024];
    //! Why the bundle was not written, empty when it was.
    char error[512];
} tl_bundle_result;

//! Write a .otio and the media it uses as an .otiod bundle: a folder of
//! the timeline as content.otio and the media under media/, the media
//! references rewritten to point there. Each file is looked for where
//! its reference says, a relative one beside the .otio, and then under
//! the media folder by name, the file whose folders match the most of
//! the end of the reference winning: an editor's paths are of its own
//! machine. An image sequence keeps a folder of its own. The output must
//! not exist, and is removed again if the bundle cannot be written or is
//! stopped. Does not need the engine, and can be called from any thread.
//! One when the bundle was written, whether media was missing or not.
TL_TIMELINE_API int tl_bundle_write_otiod(
    const char* otio,
    const char* mediaFolder,
    const char* out,
    tl_bundle_progress,
    void* progressUser,
    tl_bundle_result*);

//! Relink a .otio rather than bundle it: its media found as for a bundle,
//! nothing copied, and the timeline written to out (a .otio) with each
//! reference pointing at the file found -- by its path from the media
//! folder when it is under it, which a caller makes whole again wherever
//! the folder is next, and where it is otherwise. Missing media is left
//! as it was. For media played where it is, on a drive or a share.
TL_TIMELINE_API int tl_bundle_relink_otio(
    const char* otio,
    const char* mediaFolder,
    const char* out,
    tl_bundle_progress,
    void* progressUser,
    tl_bundle_result*);

#ifdef __cplusplus
}
#endif
