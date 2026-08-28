// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.
//
// The decode half of the WebCodecs read plugin: fetch, demux with
// mp4box.js, decode with VideoDecoder, and copy requested frames into
// the reader's image. It runs in its own worker so that nothing here
// depends on the main thread -- the page can stall or be throttled and
// decoding continues. The reader's thread and this worker share the
// wasm memory: requests arrive through a small control block and
// Atomics, and the pixels go straight into the reader's image.
//
// The control block layout (byte offsets into the wasm heap; keep in
// step with WebCodecs.cpp):
//     0  i32  request sequence number
//     4  i32  command: 1 = request a frame, 2 = close
//     8  f64  requested time, microseconds
//    16  u32  image data pointer
//    20  u32  image data capacity
//    24  i32  response sequence number (1 is the open's response)
//    32  f64  delivered time, microseconds, or < 0
//    40  i32  video width
//    44  i32  video height
//    48  i32  frame count
//    56  f64  duration, microseconds
//    64  f64  frame duration, microseconds

importScripts('mp4box.all.min.js');

var memory = null;
var readers = {};
function buf() { return memory.buffer; }

function views(ctrl)
{
    return {
        i32: new Int32Array(buf(), ctrl, 16),
        f64: new Float64Array(buf(), ctrl, 9),
        u32: new Uint32Array(buf(), ctrl, 16)
    };
}

function respond(R, seq)
{
    var v = views(R.ctrl);
    Atomics.store(v.i32, 6, seq);
    Atomics.notify(v.i32, 6);
}

// One download per file, shared by the video, audio, and thumbnail
// sessions.
var fetches = {};
function fetchFile(url)
{
    if (!fetches[url])
    {
        fetches[url] = fetch(url).then(function(r)
        {
            if (!r.ok)
            {
                throw new Error('HTTP ' + r.status);
            }
            return r.arrayBuffer();
        });
    }
    return fetches[url];
}

// The audio track decodes whole: every AAC frame is a key frame, so
// this is what buys sample accurate random access, at the cost of the
// track in memory.
function openAudio(R)
{
    function fail(e)
    {
        R.error = String(e);
        console.error('tl::webcodecs:', R.error);
        var v = views(R.ctrl);
        v.i32[10] = 0;
        v.i32[11] = 0;
        v.i32[12] = 0;
        respond(R, 1);
    }
    fetchFile(R.url).then(function(ab)
    {
        var file = MP4Box.createFile();
        file.onError = fail;
        file.onReady = function(info)
        {
            var track = info.audioTracks[0];
            if (!track)
            {
                // No audio track is an answer, not an error.
                var v = views(R.ctrl);
                v.i32[10] = 0;
                v.i32[11] = 0;
                v.i32[12] = 0;
                respond(R, 1);
                watch(R);
                return;
            }
            R.rate = track.audio.sample_rate;
            R.channels = track.audio.channel_count;
            var config = {
                codec: track.codec,
                sampleRate: R.rate,
                numberOfChannels: R.channels
            };
            // The AudioSpecificConfig, from the esds box.
            try
            {
                var esds = file.getTrackById(track.id)
                    .mdia.minf.stbl.stsd.entries[0].esds;
                config.description = esds.esd.descs[0].descs[0].data;
            }
            catch (e) {}
            R.chunks = [];
            R.adec = new AudioDecoder({
                output: function(data)
                {
                    var planes = [];
                    for (var c = 0; c < data.numberOfChannels; ++c)
                    {
                        var f = new Float32Array(data.numberOfFrames);
                        data.copyTo(f, { planeIndex: c, format: 'f32-planar' });
                        planes.push(f);
                    }
                    R.chunks.push(planes);
                    data.close();
                },
                error: fail
            });
            R.adec.configure(config);
            file.setExtractionOptions(track.id, null, { nbSamples: 1000000 });
            file.start();
        };
        file.onSamples = function(id, user, samples)
        {
            for (var i = 0; i < samples.length; ++i)
            {
                var s = samples[i];
                R.adec.decode(new EncodedAudioChunk({
                    type: 'key',
                    timestamp: Math.round(s.cts * 1e6 / s.timescale),
                    duration: Math.round(s.duration * 1e6 / s.timescale),
                    data: s.data
                }));
            }
            R.adec.flush().then(function()
            {
                var total = 0;
                for (var i = 0; i < R.chunks.length; ++i)
                {
                    total += R.chunks[i][0].length;
                }
                R.planes = [];
                for (var c = 0; c < R.channels; ++c)
                {
                    var p = new Float32Array(total);
                    var pos = 0;
                    for (var i = 0; i < R.chunks.length; ++i)
                    {
                        p.set(R.chunks[i][c] || R.chunks[i][0], pos);
                        pos += R.chunks[i][0].length;
                    }
                    R.planes.push(p);
                }
                R.chunks = null;
                R.total = total;
                var v = views(R.ctrl);
                v.i32[10] = R.rate;
                v.i32[11] = R.channels;
                v.i32[12] = total;
                v.f64[7] = total / R.rate * 1e6;
                respond(R, 1);
                watch(R);
            }).catch(fail);
        };
        ab.fileStart = 0;
        file.appendBuffer(ab);
        file.flush();
    }).catch(fail);
}

// A range of samples, interleaved into the reader's buffer; what the
// track does not cover is silence.
function requestAudio(R, seq)
{
    var v = views(R.ctrl);
    var start = Math.round(v.f64[1]);
    R.ptr = v.u32[4];
    R.cap = v.u32[5];
    var ts = -1;
    if (R.planes)
    {
        var count = Math.floor(R.cap / (R.channels * 4));
        var heap = new Float32Array(buf());
        var base = R.ptr >> 2;
        for (var i = 0; i < count; ++i)
        {
            var s = start + i;
            for (var c = 0; c < R.channels; ++c)
            {
                heap[base + i * R.channels + c] =
                    (s >= 0 && s < R.total) ? R.planes[c][s] : 0;
            }
        }
        ts = start / R.rate * 1e6;
    }
    v.i32[7] = 0;
    v.f64[4] = ts;
    respond(R, seq);
}

function open(R)
{
    function fail(e)
    {
        R.error = String(e);
        console.error('tl::webcodecs:', R.error);
        var v = views(R.ctrl);
        v.i32[10] = 0;
        v.i32[11] = 0;
        v.i32[12] = 0;
        respond(R, 1);
    }
    R.pump = function()
    {
        while (R.dec &&
            'configured' === R.dec.state &&
            R.next < R.samples.length &&
            R.dec.decodeQueueSize < 8 &&
            R.queue.length < 8)
        {
            var chunk = R.samples[R.next++];
            R.maxFedTs = Math.max(R.maxFedTs, chunk.timestamp);
            R.dec.decode(chunk);
        }
        if (R.dec &&
            'configured' === R.dec.state &&
            R.next >= R.samples.length &&
            R.samples.length > 0 &&
            !R.flushed)
        {
            R.flushed = true;
            R.dec.flush().then(function()
            {
                R.eos = true;
                R.service();
            }).catch(function() {});
        }
    };
    R.push = function(item)
    {
        var i = R.queue.length;
        while (i > 0 && R.queue[i - 1].ts > item.ts)
        {
            --i;
        }
        R.queue.splice(i, 0, item);
    };
    R.service = function()
    {
        if (R.target < 0)
        {
            return;
        }
        var t = R.target;
        while (R.queue.length >= 2 && R.queue[1].ts <= t)
        {
            R.queue.shift();
            R.pump();
        }
        var head = R.queue.length ? R.queue[0] : null;
        var proven = head && head.ts <= t &&
            (R.queue.length >= 2 ?
                R.queue[1].ts > t :
                (R.eos && R.next >= R.samples.length) ||
                    t - head.ts < R.frameDur);
        if (!proven)
        {
            R.pump();
            return;
        }
        var f = R.queue.shift();
        R.target = -1;
        R.floor = f.ts;
        var ts = f.ts;
        var fmt = 0;
        var heap = new Uint8Array(buf());
        var w = R.w;
        var h = R.h;
        var cw = (w + 1) >> 1;
        var ch = (h + 1) >> 1;
        var L = f.layout;
        // The frame is top down and the image is bottom up, so every
        // plane's rows are written in reverse.
        function rows(srcOff, srcStride, dstOff, rowBytes, rowCount)
        {
            for (var y = 0; y < rowCount; ++y)
            {
                var s = srcOff + y * srcStride;
                heap.set(
                    f.buf.subarray(s, s + rowBytes),
                    dstOff + (rowCount - 1 - y) * rowBytes);
            }
        }
        if (!L || 1 === L.length)
        {
            var stride = L ? L[0].stride : w * 4;
            var off = L ? L[0].offset : 0;
            if (w * 4 * h <= R.cap &&
                off + (h - 1) * stride + w * 4 <= f.buf.length)
            {
                rows(off, stride, R.ptr, w * 4, h);
            }
            else
            {
                ts = -1;
            }
        }
        else if (w * h + cw * ch * 2 <= R.cap)
        {
            // The native planes become planar 4:2:0; NV12's
            // interleaved chroma is split on the way.
            fmt = 1;
            rows(L[0].offset, L[0].stride, R.ptr, w, h);
            var uOff = R.ptr + w * h;
            var vOff = uOff + cw * ch;
            if (2 === L.length)
            {
                for (var y = 0; y < ch; ++y)
                {
                    var s = L[1].offset + y * L[1].stride;
                    var dU = uOff + (ch - 1 - y) * cw;
                    var dV = vOff + (ch - 1 - y) * cw;
                    for (var x = 0; x < cw; ++x)
                    {
                        heap[dU + x] = f.buf[s + x * 2];
                        heap[dV + x] = f.buf[s + x * 2 + 1];
                    }
                }
            }
            else
            {
                rows(L[1].offset, L[1].stride, uOff, cw, ch);
                rows(L[2].offset, L[2].stride, vOff, cw, ch);
            }
        }
        else
        {
            ts = -1;
        }
        var v = views(R.ctrl);
        v.i32[7] = fmt;
        v.f64[4] = ts;
        respond(R, R.seq);
        R.pump();
    };
    function fallbackConvert(frame)
    {
        // Safari: the copy does not convert, and drawing a VideoFrame
        // does not work everywhere either -- an ImageBitmap does.
        var w = frame.displayWidth;
        var h = frame.displayHeight;
        var ts = frame.timestamp;
        createImageBitmap(frame).then(function(bmp)
        {
            frame.close();
            if (!R.cnv)
            {
                R.cnv = new OffscreenCanvas(w, h);
                R.ctx = R.cnv.getContext('2d', { willReadFrequently: true });
            }
            R.ctx.drawImage(bmp, 0, 0);
            bmp.close();
            var d = R.ctx.getImageData(0, 0, w, h);
            R.push({ ts: ts, buf: new Uint8Array(d.data.buffer) });
            R.service();
        }).catch(function(e)
        {
            console.error('tl::webcodecs: convert:', e);
            frame.close();
            R.pump();
        });
    }
    function onFrame(frame)
    {
        // The frames on the way to a seek target are decode work only:
        // dropped before the conversion, which is the expensive step.
        if (R.target >= 0 && frame.timestamp <= R.target - R.frameDur)
        {
            frame.close();
            R.pump();
            return;
        }
        var opts = { format: 'RGBA' };
        var size = 0;
        try { size = frame.allocationSize(opts); } catch (e) {}
        if (size > 0)
        {
            var buf = new Uint8Array(size);
            frame.copyTo(buf, opts).then(function(layout)
            {
                // Safari resolves the copy without converting; the
                // layout is the truth of what was written, and the
                // native planes are delivered as they are -- the
                // renderer draws planar YUV itself.
                R.push({
                    ts: frame.timestamp,
                    buf: buf,
                    fmt: frame.format,
                    layout: layout });
                frame.close();
                R.service();
            }).catch(function() { fallbackConvert(frame); });
        }
        else
        {
            fallbackConvert(frame);
        }
    }
    function description(trak)
    {
        var entry = trak.mdia.minf.stbl.stsd.entries[0];
        var box = entry.avcC || entry.hvcC || entry.vpcC || entry.av1C;
        if (!box)
        {
            return null;
        }
        var stream = new DataStream(undefined, 0, DataStream.BIG_ENDIAN);
        box.write(stream);
        // Past the box header.
        return new Uint8Array(stream.buffer, 8);
    }
    fetchFile(R.url).then(function(ab)
    {
        var file = MP4Box.createFile();
        file.onError = fail;
        file.onReady = function(info)
        {
            var track = info.videoTracks[0];
            if (!track)
            {
                fail('no video track');
                return;
            }
            R.w = track.video.width;
            R.h = track.video.height;
            R.dur = track.duration / track.timescale * 1e6;
            R.config = {
                codec: track.codec,
                codedWidth: R.w,
                codedHeight: R.h
            };
            var desc = description(file.getTrackById(track.id));
            if (desc)
            {
                R.config.description = desc;
            }
            R.dec = new VideoDecoder({
                output: onFrame,
                error: fail
            });
            // The decoder can want more input before its first output;
            // this is what feeds it when the queue drains.
            R.dec.ondequeue = function() { R.pump(); };
            R.dec.configure(R.config);
            file.setExtractionOptions(track.id, null, { nbSamples: 1000000 });
            file.start();
        };
        file.onSamples = function(id, user, samples)
        {
            // The composition times carry the B frame delay; the edit
            // list is what normally corrects it, so the smallest time
            // is subtracted from all of them.
            var offset = Infinity;
            for (var i = 0; i < samples.length; ++i)
            {
                offset = Math.min(offset, samples[i].cts);
            }
            for (var i = 0; i < samples.length; ++i)
            {
                var s = samples[i];
                R.samples.push(new EncodedVideoChunk({
                    type: s.is_sync ? 'key' : 'delta',
                    timestamp: Math.round((s.cts - offset) * 1e6 / s.timescale),
                    duration: Math.round(s.duration * 1e6 / s.timescale),
                    data: s.data
                }));
            }
            if (R.samples.length)
            {
                R.frameDur = R.samples[0].duration || 42000;
            }
            var v = views(R.ctrl);
            v.i32[10] = R.w;
            v.i32[11] = R.h;
            v.i32[12] = R.samples.length;
            v.f64[7] = R.dur;
            v.f64[8] = R.frameDur;
            respond(R, 1);
            watch(R);
        };
        ab.fileStart = 0;
        file.appendBuffer(ab);
        file.flush();
    }).catch(fail);
}

function request(R, seq)
{
    var v = views(R.ctrl);
    var t = v.f64[1];
    R.seq = seq;
    R.target = t;
    R.ptr = v.u32[4];
    R.cap = v.u32[5];
    // The keyframe at or before the time. Decode continues forward
    // when it can: backward, or forward past a keyframe that has not
    // been fed yet, restarts there instead -- resetting the decoder is
    // what costs, and decoding across a skipped gap costs more.
    var key = 0;
    for (var i = 0; i < R.samples.length; ++i)
    {
        if (R.samples[i].timestamp > t)
        {
            break;
        }
        if ('key' === R.samples[i].type)
        {
            key = i;
        }
    }
    var floorTs = R.queue.length ? R.queue[0].ts : R.floor;
    if (t < floorTs || key > R.next)
    {
        R.next = key;
        R.flushed = false;
        R.eos = false;
        R.maxFedTs = -1;
        R.queue = [];
        R.dec.reset();
        R.dec.configure(R.config);
    }
    R.service();
}

// Wait for the next command. Atomics.waitAsync where the browser has
// it; a short poll where it does not.
function watch(R)
{
    var v = views(R.ctrl);
    // Starts at zero rather than the current value: a command can land
    // between the open response and this watch arming, and it must not
    // read as already seen.
    var seen = 0;
    function handle()
    {
        var seq = Atomics.load(v.i32, 0);
        if (seq !== seen)
        {
            seen = seq;
            var command = Atomics.load(v.i32, 1);
            if (1 === command)
            {
                if (R.isAudio)
                {
                    requestAudio(R, seq);
                }
                else
                {
                    request(R, seq);
                }
            }
            else if (2 === command)
            {
                if (R.dec)
                {
                    try { R.dec.close(); } catch (e) {}
                }
                delete readers[R.handle];
                return;
            }
        }
        wait();
    }
    // A plain poll: Atomics.waitAsync misses wakes now and then in
    // practice, and a few milliseconds of latency is nothing next to a
    // decode.
    function wait()
    {
        setTimeout(handle, 4);
    }
    wait();
}

onmessage = function(event)
{
    var data = event.data;
    if (data.memory)
    {
        memory = data.memory;
    }
    if (data.url && data.audio)
    {
        var A = {
            handle: data.handle,
            ctrl: data.ctrl,
            url: data.url,
            isAudio: true,
            planes: null,
            total: 0,
            rate: 0,
            channels: 0,
            ptr: 0,
            cap: 0,
            error: ''
        };
        readers[data.handle] = A;
        openAudio(A);
    }
    else if (data.url)
    {
        var R = {
            handle: data.handle,
            ctrl: data.ctrl,
            url: data.url,
            samples: [],
            queue: [],
            next: 0,
            dec: null,
            config: null,
            flushed: false,
            eos: false,
            maxFedTs: -1,
            floor: -1,
            frameDur: 42000,
            w: 0,
            h: 0,
            dur: 0,
            target: -1,
            seq: 0,
            ptr: 0,
            cap: 0,
            error: ''
        };
        readers[data.handle] = R;
        open(R);
    }
};
