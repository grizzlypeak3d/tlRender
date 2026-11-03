/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright Contributors to the tlRender project. */

/* The C interface (tlC), from C: what Swift sees, and what DJV Studio for
 * the iPad is built on. Written in C, so the header is checked as C as
 * well as what it does. Everything but the views, which need a window and
 * the GPU renderer: the engine still opens and reads files without them,
 * so this runs on a machine with neither. */

#include <tlRender/C/Engine.h>

#include <SDL3/SDL_timer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(x) check((x), #x, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, e) checkNear((a), (b), (e), #a, __FILE__, __LINE__)

static void check(int value, const char* text, const char* file, int line)
{
    if (!value)
    {
        printf("FAILED: %s (%s:%d)\n", text, file, line);
        ++failures;
    }
}

static void checkNear(double a, double b, double e, const char* text, const char* file, int line)
{
    if (fabs(a - b) > e)
    {
        printf("FAILED: %s is %f, not %f (%s:%d)\n", text, a, b, file, line);
        ++failures;
    }
}

static void path(char* out, size_t size, const char* dir, const char* name)
{
    snprintf(out, size, "%s/%s", dir, name);
}

/* Poll the engine until a request is done: the work is in the background,
 * and some of it is collected as the engine ticks. */
#define WAIT(engine, done) \
    for (int ms = 0; !(done) && ms < 20000; ms += 10) \
    { \
        tl_engine_tick(engine); \
        SDL_Delay(10); \
    }

static void players(tl_engine* engine)
{
    char file[1024];

    /* Three movies one after another, with their audio. */
    path(file, sizeof(file), TLRENDER_SAMPLE_DATA, "MultipleClips.otio");
    tl_player* player = tl_player_create(engine, file);
    CHECK(player != NULL);
    if (!player)
        return;
    const double rate = tl_player_get_rate(player);
    const double frame = rate > 0.0 ? 1.0 / rate : 0.0;
    CHECK(rate > 23.0 && rate < 31.0);
    CHECK_NEAR(tl_player_get_duration(player), 9.0, 0.1);
    CHECK(tl_player_get_video_width(player) > 0);
    CHECK(tl_player_get_video_height(player) > 0);
    CHECK_NEAR(tl_player_get_speed(player), rate, 0.001);

    CHECK(tl_player_get_video_track_count(player) == 1);
    CHECK(tl_player_get_clip_count(player) == 3);
    tl_clip clip;
    CHECK(tl_player_get_clip(player, 1, &clip) == 1);
    CHECK(clip.track == 0);
    CHECK_NEAR(clip.start, 3.0, 0.1);
    CHECK_NEAR(clip.duration, 3.0, 0.1);
    CHECK(strcmp(clip.name, "Dinky_2015-06-11") == 0);
    CHECK(!clip.hasColor);
    CHECK(tl_player_get_clip(player, 3, &clip) == 0);
    CHECK(tl_player_get_clip(player, -1, &clip) == 0);

    CHECK(tl_player_get_shot_count(player) == 3);
    tl_shot shot;
    CHECK(tl_player_get_shot(player, 2, &shot) == 1);
    CHECK(shot.clip == 2);
    CHECK_NEAR(shot.start, 6.0, 0.1);
    CHECK(strcmp(shot.name, "BART_2021-02-07") == 0);

    /* Moving about. */
    tl_player_seek(player, 4.0);
    CHECK_NEAR(tl_player_get_time(player), 4.0, frame);
    tl_player_time_action(player, TL_TIME_START);
    CHECK_NEAR(tl_player_get_time(player), 0.0, 0.0001);
    tl_player_time_action(player, TL_TIME_FRAME_NEXT);
    CHECK_NEAR(tl_player_get_time(player), frame, 0.0001);
    tl_player_time_action(player, TL_TIME_END);
    CHECK_NEAR(tl_player_get_time(player), tl_player_get_duration(player) - frame, 0.0001);
    tl_player_time_action(player, TL_TIME_JUMP_BACK_1S);
    CHECK_NEAR(tl_player_get_time(player), tl_player_get_duration(player) - frame - 1.0, frame);

    /* The in/out range, which Loop Shot sets to a shot. */
    double start = -1.0;
    double end = -1.0;
    tl_player_set_in_out(player, 3.0, 6.0);
    tl_player_get_in_out(player, &start, &end);
    CHECK_NEAR(start, 3.0, frame);
    CHECK_NEAR(end, 6.0, frame);
    tl_player_reset_in_point(player);
    tl_player_reset_out_point(player);
    tl_player_get_in_out(player, &start, &end);
    CHECK_NEAR(start, 0.0, 0.0001);
    CHECK_NEAR(end, tl_player_get_duration(player), frame);
    tl_player_set_loop(player, TL_LOOP_ONCE);
    CHECK(tl_player_get_loop(player) == TL_LOOP_ONCE);
    tl_player_set_loop(player, TL_LOOP_LOOP);
    CHECK(tl_player_get_loop(player) == TL_LOOP_LOOP);
    CHECK(tl_player_get_playback(player) == TL_PLAYBACK_STOP);

    /* A thumbnail, as the wall's cards ask for them. */
    unsigned long long id = tl_player_request_thumbnail(player, 1.0, 64);
    CHECK(id != 0);
    tl_image image;
    memset(&image, 0, sizeof(image));
    int ready = 0;
    WAIT(engine, (ready = tl_player_get_thumbnail(player, id, &image)) != 0);
    CHECK(ready == 1);
    if (1 == ready)
    {
        CHECK(image.height == 64);
        CHECK(image.width > 0);
        CHECK(image.data != NULL);
        tl_image_free(&image);
    }

    /* The waveform under the cards. */
    float columns[2 * 100];
    id = tl_player_request_waveform(player, 0.0, 9.0, 100);
    CHECK(id != 0);
    ready = 0;
    WAIT(engine, (ready = tl_player_get_waveform(player, id, columns, 100)) != 0);
    CHECK(ready == 1);
    if (1 == ready)
    {
        int sound = 0;
        for (int i = 0; i < 2 * 100; ++i)
        {
            sound |= columns[i] != 0.F;
        }
        CHECK(sound);
    }
    tl_player_destroy(player);

    /* A clip the editor colored, and one left alone. */
    path(file, sizeof(file), TLRENDER_SAMPLE_DATA, "ClipColors.otio");
    player = tl_player_create(engine, file);
    CHECK(player != NULL);
    if (player)
    {
        CHECK(tl_player_get_clip(player, 0, &clip) == 1);
        CHECK(clip.hasColor);
        CHECK_NEAR(clip.color[0], 0.75, 0.001);
        CHECK_NEAR(clip.color[1], 0.25, 0.001);
        CHECK_NEAR(clip.color[2], 0.125, 0.001);
        CHECK(tl_player_get_shot(player, 0, &shot) == 1);
        CHECK(shot.hasColor);
        CHECK(tl_player_get_clip(player, 1, &clip) == 1);
        CHECK(!clip.hasColor);
        tl_player_destroy(player);
    }

    /* Markers on the timeline, its tracks and its clips, in time order. */
    path(file, sizeof(file), TLRENDER_SAMPLE_DATA, "Markers.otio");
    player = tl_player_create(engine, file);
    CHECK(player != NULL);
    if (player)
    {
        const int count = tl_player_get_marker_count(player);
        printf("Markers: %d\n", count);
        CHECK(count >= 4);
        /* A clip's markers are in its media's time, so one before the
         * part of the media the clip uses is before the clip. */
        int red = 0;
        double previous = -1.0e9;
        for (int i = 0; i < count; ++i)
        {
            tl_marker marker;
            CHECK(tl_player_get_marker(player, i, &marker) == 1);
            CHECK(marker.start >= previous);
            previous = marker.start;
            if (strcmp(marker.name, "Marker 0") == 0)
            {
                red = marker.color[0] > .5F && marker.color[1] < .5F && marker.color[2] < .5F;
            }
        }
        CHECK(red);
        tl_player_destroy(player);
    }

    /* What cannot be opened says why. */
    path(file, sizeof(file), TLRENDER_SAMPLE_DATA, "NotThere.otio");
    CHECK(tl_player_create(engine, file) == NULL);
    CHECK(strlen(tl_engine_get_error(engine)) > 0);
}

static void cuts(tl_engine* engine)
{
    char file[1024];
    path(file, sizeof(file), TLRENDER_SAMPLE_DATA, "MultipleClips.otio");
    tl_cut* cut = tl_cut_open(engine, file);
    CHECK(cut != NULL);
    if (!cut)
        return;
    CHECK_NEAR(tl_cut_get_duration(cut), 9.0, 0.1);
    CHECK(tl_cut_get_rate(cut) > 0.0);
    CHECK(tl_cut_get_video_width(cut) > 0);
    CHECK(tl_cut_get_clip_count(cut) == 3);
    CHECK(tl_cut_get_shot_count(cut) == 3);
    tl_clip clip;
    CHECK(tl_cut_get_clip(cut, 0, &clip) == 1);
    CHECK(strcmp(clip.name, "PSR63_2012-06-02") == 0);

    /* The library card's poster, which outlives the cut. */
    const unsigned long long id = tl_cut_request_poster(cut, 0.0, 48);
    tl_cut_close(cut);
    CHECK(id != 0);
    tl_image image;
    memset(&image, 0, sizeof(image));
    int ready = 0;
    WAIT(engine, (ready = tl_engine_get_poster(engine, id, &image)) != 0);
    CHECK(ready == 1);
    if (1 == ready)
    {
        CHECK(image.height == 48);
        tl_image_free(&image);
    }
}

static int progressCalls = 0;

static int progress(double fraction, void* user)
{
    (void)fraction;
    (void)user;
    ++progressCalls;
    return 1;
}

static int stop(double fraction, void* user)
{
    (void)fraction;
    (void)user;
    return 0;
}

/* A cut with a clip whose file is nowhere, and one found under the media
 * folder by its name. */
static const char* missingOTIO =
    "{\"OTIO_SCHEMA\": \"Timeline.1\", \"name\": \"Missing\", \"tracks\": {"
    "\"OTIO_SCHEMA\": \"Stack.1\", \"name\": \"tracks\", \"children\": [{"
    "\"OTIO_SCHEMA\": \"Track.1\", \"name\": \"Video\", \"kind\": \"Video\", \"children\": ["
    "{\"OTIO_SCHEMA\": \"Clip.1\", \"name\": \"NotThere\", \"source_range\": {"
    "\"OTIO_SCHEMA\": \"TimeRange.1\","
    "\"start_time\": {\"OTIO_SCHEMA\": \"RationalTime.1\", \"value\": 0, \"rate\": 24},"
    "\"duration\": {\"OTIO_SCHEMA\": \"RationalTime.1\", \"value\": 24, \"rate\": 24}},"
    "\"media_reference\": {\"OTIO_SCHEMA\": \"ExternalReference.1\", \"target_url\": \"NotThere.mov\"}},"
    "{\"OTIO_SCHEMA\": \"Clip.1\", \"name\": \"ColorPattern\", \"source_range\": {"
    "\"OTIO_SCHEMA\": \"TimeRange.1\","
    "\"start_time\": {\"OTIO_SCHEMA\": \"RationalTime.1\", \"value\": 0, \"rate\": 24},"
    "\"duration\": {\"OTIO_SCHEMA\": \"RationalTime.1\", \"value\": 24, \"rate\": 24}},"
    "\"media_reference\": {\"OTIO_SCHEMA\": \"ExternalReference.1\", \"target_url\": \"ColorPattern.png\"}}"
    "]}]}}";

static void bundles(tl_engine* engine)
{
    char otio[1024];
    char out[1024];
    tl_bundle_result result;

    /* A .otio and its media as an .otiod, which opens by its folder. */
    path(otio, sizeof(otio), TLRENDER_SAMPLE_DATA, "MultipleClips.otio");
    path(out, sizeof(out), TL_C_TEST_OUT, "MultipleClips.otiod");
    memset(&result, 0, sizeof(result));
    CHECK(tl_bundle_write_otiod(otio, TLRENDER_SAMPLE_DATA, out, progress, NULL, &result) == 1);
    CHECK(result.media == 3);
    CHECK(result.missing == 0);
    CHECK(strlen(result.error) == 0);
    CHECK(progressCalls > 0);
    tl_cut* cut = tl_cut_open(engine, out);
    CHECK(cut != NULL);
    if (cut)
    {
        CHECK_NEAR(tl_cut_get_duration(cut), 9.0, 0.1);
        CHECK(tl_cut_get_clip_count(cut) == 3);
        tl_cut_close(cut);
    }
    tl_player* player = tl_player_create(engine, out);
    CHECK(player != NULL);
    if (player)
    {
        CHECK(tl_player_get_video_width(player) > 0);
        tl_player_destroy(player);
    }

    /* Written once: the output must not be there already. */
    memset(&result, 0, sizeof(result));
    CHECK(tl_bundle_write_otiod(otio, TLRENDER_SAMPLE_DATA, out, NULL, NULL, &result) == 0);
    CHECK(strlen(result.error) > 0);

    /* Relinked rather than copied. */
    path(out, sizeof(out), TL_C_TEST_OUT, "MultipleClips.otio");
    memset(&result, 0, sizeof(result));
    CHECK(tl_bundle_relink_otio(otio, TLRENDER_SAMPLE_DATA, out, NULL, NULL, &result) == 1);
    CHECK(result.media == 3);
    CHECK(result.missing == 0);

    /* Missing media is said, and the bundle is still written. */
    path(otio, sizeof(otio), TL_C_TEST_OUT, "Missing.otio");
    FILE* f = fopen(otio, "w");
    CHECK(f != NULL);
    if (f)
    {
        fputs(missingOTIO, f);
        fclose(f);
    }
    path(out, sizeof(out), TL_C_TEST_OUT, "Missing.otiod");
    memset(&result, 0, sizeof(result));
    CHECK(tl_bundle_write_otiod(otio, TLRENDER_SAMPLE_DATA, out, NULL, NULL, &result) == 1);
    CHECK(result.media == 2);
    CHECK(result.missing == 1);
    CHECK(strstr(result.missingNames, "NotThere.mov") != NULL);

    /* Stopped part way, and nothing left behind. */
    path(otio, sizeof(otio), TLRENDER_SAMPLE_DATA, "MultipleClips.otio");
    path(out, sizeof(out), TL_C_TEST_OUT, "Stopped.otiod");
    memset(&result, 0, sizeof(result));
    CHECK(tl_bundle_write_otiod(otio, TLRENDER_SAMPLE_DATA, out, stop, NULL, &result) == 0);
    CHECK(tl_cut_open(engine, out) == NULL);
}

int main(void)
{
    tl_engine* engine = tl_engine_create();
    CHECK(engine != NULL);
    if (!engine)
        return 1;
    printf("GPU renderer: %s\n", tl_engine_has_gpu(engine) ? "yes" : "no");
    if (!tl_engine_has_gpu(engine))
    {
        printf("    %s\n", tl_engine_get_error(engine));
    }

    players(engine);
    cuts(engine);
    bundles(engine);

    tl_engine_destroy(engine);
    printf("%s: %d failed\n", failures ? "FAILED" : "Passed", failures);
    return failures ? 1 : 0;
}
