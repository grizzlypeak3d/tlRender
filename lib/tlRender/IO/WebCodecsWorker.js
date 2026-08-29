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

// One source per file, shared by the video, audio, and thumbnail
// sessions. A small file, or a server that ignores ranges, is a
// single download; a large file on a range-capable server is read
// through a cache of fetched blocks, so the sessions share the bytes
// and only what plays is downloaded.
var sources = {};
var PROBE = 64 * 1024;
var WHOLE_MAX = 32 * 1024 * 1024;
var BLOCK = 1024 * 1024;
var CACHE_BLOCKS = 32;

function wholeSource(url)
{
    return fetch(url).then(function(r)
    {
        if (!r.ok)
        {
            throw new Error('HTTP ' + r.status);
        }
        return r.arrayBuffer();
    }).then(function(ab)
    {
        return { url: url, buffer: ab, size: ab.byteLength };
    });
}

function getSource(url)
{
    if (!sources[url])
    {
        // The browser HTTP cache is bypassed: the block cache is the
        // cache here, and Firefox serializes ranged requests against
        // a partial cache entry -- the end of the file took tens of
        // seconds to arrive.
        sources[url] = fetch(
            url,
            {
                headers: { 'Range': 'bytes=0-' + (PROBE - 1) },
                cache: 'no-store'
            })
        .then(function(r)
        {
            if (206 === r.status)
            {
                // The size is a nicety, not a requirement: Content-Range
                // is only readable across origins when the server
                // exposes it, and a HEAD is refused by a host whose
                // CORS allows only GET. Unknown means streaming with
                // reads past the end answered by 416.
                var m = /\/(\d+)\s*$/.exec(
                    r.headers.get('Content-Range') || '');
                var size = m ? parseInt(m[1]) : 0;
                var sizeP = size ? Promise.resolve(size) :
                    fetch(url, { method: 'HEAD' }).then(function(h)
                    {
                        return parseInt(
                            h.headers.get('Content-Length')) || 0;
                    }, function() { return 0; });
                return Promise.all([r.arrayBuffer(), sizeP])
                .then(function(a)
                {
                    if (a[1] && a[1] <= WHOLE_MAX)
                    {
                        return wholeSource(url);
                    }
                    return {
                        url: url,
                        buffer: null,
                        size: a[1] ? a[1] : Infinity,
                        head: a[0],
                        blocks: new Map()
                    };
                });
            }
            if (!r.ok)
            {
                throw new Error('HTTP ' + r.status);
            }
            // The server ignored the range; the response is the file.
            return r.arrayBuffer().then(function(ab)
            {
                return { url: url, buffer: ab, size: ab.byteLength };
            });
        }, function()
        {
            // A cross origin host can allow plain requests but reject
            // the preflight that the Range header brings on.
            return wholeSource(url);
        });
    }
    return sources[url];
}

function readBlock(S, b)
{
    var p = S.blocks.get(b);
    if (p)
    {
        // Refreshed for the eviction order.
        S.blocks.delete(b);
    }
    else
    {
        var start = b * BLOCK;
        var end = Math.min(start + BLOCK, S.size);
        p = fetch(
            S.url,
            {
                headers: { 'Range': 'bytes=' + start + '-' + (end - 1) },
                cache: 'no-store'
            })
        .then(function(r)
        {
            if (416 === r.status)
            {
                // Past the end of a file whose size is unknown.
                return new ArrayBuffer(0);
            }
            if (206 !== r.status)
            {
                throw new Error('HTTP ' + r.status);
            }
            return r.arrayBuffer();
        });
    }
    S.blocks.set(b, p);
    while (S.blocks.size > CACHE_BLOCKS)
    {
        S.blocks.delete(S.blocks.keys().next().value);
    }
    return p;
}

// A copy of the byte range, assembled from the cache.
function readRange(S, offset, size)
{
    var end = Math.min(offset + size, S.size);
    if (offset >= end)
    {
        return Promise.resolve(new ArrayBuffer(0));
    }
    if (S.buffer)
    {
        return Promise.resolve(S.buffer.slice(offset, end));
    }
    if (S.head && end <= S.head.byteLength)
    {
        return Promise.resolve(S.head.slice(offset, end));
    }
    var b0 = Math.floor(offset / BLOCK);
    var b1 = Math.floor((end - 1) / BLOCK);
    var blocks = [];
    for (var b = b0; b <= b1; ++b)
    {
        blocks.push(readBlock(S, b));
    }
    return Promise.all(blocks).then(function(abs)
    {
        var out = new Uint8Array(end - offset);
        for (var i = 0; i < abs.length; ++i)
        {
            var bStart = (b0 + i) * BLOCK;
            var src = new Uint8Array(abs[i]);
            var lo = Math.max(offset, bStart);
            var hi = Math.min(end, bStart + src.length);
            out.set(
                src.subarray(lo - bStart, hi - bStart),
                lo - offset);
        }
        return out.buffer;
    });
}

// The demuxer only ever sees the file's structure: the top level
// boxes are walked by hand and the demuxer is handed the ftyp and the
// moov as one contiguous stream, so the index costs the moov and not
// the media -- wherever in the file the moov is. The sample offsets
// are absolute, so the media reads are unaffected by the splice.
function parseIndex(S)
{
    if (!S.index)
    {
        var ftyp = null;
        var moov = null;
        function walk(off)
        {
            if (off + 16 > S.size)
            {
                return Promise.reject(new Error('no moov'));
            }
            return readRange(S, off, 16).then(function(ab)
            {
                var v = new DataView(ab);
                var size = v.getUint32(0);
                var type = String.fromCharCode(
                    v.getUint8(4), v.getUint8(5),
                    v.getUint8(6), v.getUint8(7));
                if (1 === size)
                {
                    size = Number(v.getBigUint64(8));
                }
                else if (0 === size)
                {
                    // A box to the end of the file; nothing follows,
                    // and a read past an unknown end also lands here.
                    if ('moov' !== type)
                    {
                        return Promise.reject(new Error('no moov'));
                    }
                    size = S.size - off;
                }
                if ('ftyp' === type)
                {
                    ftyp = { off: off, size: size };
                }
                if ('moov' === type)
                {
                    moov = { off: off, size: size };
                    return;
                }
                return walk(off + size);
            });
        }
        S.index = walk(0).then(function()
        {
            if (!isFinite(moov.size))
            {
                return Promise.reject(new Error('no moov size'));
            }
            return Promise.all([
                ftyp ? readRange(S, ftyp.off, ftyp.size) : null,
                readRange(S, moov.off, moov.size)]);
        }).then(function(boxes)
        {
            return new Promise(function(resolve, reject)
            {
                var file = MP4Box.createFile();
                var done = false;
                file.onError = function(e)
                {
                    if (!done) { done = true; reject(new Error(String(e))); }
                };
                file.onReady = function(info)
                {
                    if (!done)
                    {
                        done = true;
                        resolve({ file: file, info: info });
                    }
                };
                var pos = 0;
                for (var i = 0; i < boxes.length; ++i)
                {
                    if (boxes[i])
                    {
                        boxes[i].fileStart = pos;
                        pos += boxes[i].byteLength;
                        file.appendBuffer(boxes[i]);
                    }
                }
                file.flush();
                if (!done)
                {
                    done = true;
                    reject(new Error('no index'));
                }
            });
        }).then(function(ix)
        {
            // When the server would not say the size, the index says
            // it: nothing past the last sample is ever wanted, and a
            // range past the real end of the file stalled for tens of
            // seconds in Firefox.
            if (!isFinite(S.size))
            {
                var end = moov.off + moov.size;
                var tracks = [].concat(
                    ix.info.videoTracks || [], ix.info.audioTracks || []);
                for (var i = 0; i < tracks.length; ++i)
                {
                    var samples = ix.file.getTrackById(tracks[i].id).samples;
                    for (var j = 0; j < samples.length; ++j)
                    {
                        end = Math.max(
                            end, samples[j].offset + samples[j].size);
                    }
                }
                S.size = end;
            }
            // The tail is wanted the moment a player exists: the
            // loop's read-behind wraps the first frame to the end of
            // the file, and that fetch pending while the main thread
            // stalls deadlocked in Firefox until the read timeouts.
            // Fetched here, it is in the cache before anything waits.
            if (!S.buffer && isFinite(S.size) && S.size > 0)
            {
                readBlock(S, Math.floor((S.size - 1) / BLOCK));
            }
            return ix;
        });
    }
    return S.index;
}

function openAudio(A)
{
    function fail(e)
    {
        A.error = String(e);
        console.error('tl::webcodecs:', A.error);
        var v = views(A.ctrl);
        v.i32[10] = 0;
        v.i32[11] = 0;
        v.i32[12] = 0;
        respond(A, 1);
    }
    getSource(A.url).then(function(S)
    {
        A.src = S;
        return parseIndex(S);
    }).then(function(ix)
    {
        var track = ix.info.audioTracks[0];
        if (!track)
        {
            // No audio track is an answer, not an error.
            var v = views(A.ctrl);
            v.i32[10] = 0;
            v.i32[11] = 0;
            v.i32[12] = 0;
            respond(A, 1);
            watch(A);
            return;
        }
        A.rate = track.audio.sample_rate;
        A.channels = track.audio.channel_count;
        A.config = {
            codec: track.codec,
            sampleRate: A.rate,
            numberOfChannels: A.channels
        };
        // The AudioSpecificConfig, from the esds box.
        try
        {
            var esds = ix.file.getTrackById(track.id)
                .mdia.minf.stbl.stsd.entries[0].esds;
            A.config.description = esds.esd.descs[0].descs[0].data;
        }
        catch (e) {}
        var raw = ix.file.getTrackById(track.id).samples;
        A.frames = [];
        for (var i = 0; i < raw.length; ++i)
        {
            var s = raw[i];
            A.frames.push({
                off: s.offset,
                size: s.size,
                pos: Math.round(s.dts * A.rate / s.timescale)
            });
        }
        var last = raw.length ? raw[raw.length - 1] : null;
        A.total = last ?
            Math.round((last.dts + last.duration) * A.rate / last.timescale) :
            0;
        A.cache = {};
        var v = views(A.ctrl);
        v.i32[10] = A.rate;
        v.i32[11] = A.channels;
        v.i32[12] = A.total;
        v.f64[7] = A.total / A.rate * 1e6;
        respond(A, 1);
        watch(A);
    }).catch(fail);
}

// The last frame starting at or before the position.
function frameForPos(A, pos)
{
    var lo = 0;
    var hi = A.frames.length - 1;
    while (lo < hi)
    {
        var mid = (lo + hi + 1) >> 1;
        if (A.frames[mid].pos <= pos)
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1;
        }
    }
    return lo;
}

// Decode the frames covering the range into the cache. Every AAC
// frame is a key frame, so any window is decodable on its own; the
// one extra leading frame warms the decoder's overlap.
function ensureAudio(A, start, count)
{
    if (!A.frames.length || start >= A.total || start + count <= 0)
    {
        return Promise.resolve();
    }
    var f0 = frameForPos(A, Math.max(0, start));
    var f1 = frameForPos(A, Math.min(A.total, start + count) - 1);
    var missing = false;
    for (var i = f0; i <= f1; ++i)
    {
        if (!A.cache[i]) { missing = true; break; }
    }
    if (!missing)
    {
        return Promise.resolve();
    }
    var d0 = Math.max(0, f0 - 1);
    var reads = [];
    for (var i = d0; i <= f1; ++i)
    {
        (function(i)
        {
            reads.push(
                readRange(A.src, A.frames[i].off, A.frames[i].size)
                .then(function(ab) { return ab; }));
        })(i);
    }
    return Promise.all(reads).then(function(datas)
    {
        return new Promise(function(resolve, reject)
        {
            var batch = [];
            if (!A.adec || 'closed' === A.adec.state)
            {
                A.adec = new AudioDecoder({
                    output: function(data)
                    {
                        var planes = [];
                        for (var c = 0; c < data.numberOfChannels; ++c)
                        {
                            var f = new Float32Array(data.numberOfFrames);
                            data.copyTo(
                                f, { planeIndex: c, format: 'f32-planar' });
                            planes.push(f);
                        }
                        A.batch.push({
                            pos: Math.round(data.timestamp * A.rate / 1e6),
                            len: data.numberOfFrames,
                            planes: planes
                        });
                        data.close();
                    },
                    error: function(e)
                    {
                        console.error('tl::webcodecs:', String(e));
                    }
                });
            }
            else
            {
                A.adec.reset();
            }
            A.batch = batch;
            A.adec.configure(A.config);
            for (var i = d0; i <= f1; ++i)
            {
                A.adec.decode(new EncodedAudioChunk({
                    type: 'key',
                    timestamp: Math.round(A.frames[i].pos / A.rate * 1e6),
                    data: datas[i - d0]
                }));
            }
            A.adec.flush().then(function()
            {
                for (var i = 0; i < batch.length; ++i)
                {
                    A.cache[frameForPos(A, batch[i].pos)] = batch[i];
                }
                // What is outside the request will be decoded again if
                // it is ever wanted again. The whole request stays: a
                // waveform reads tens of seconds at once, and evicting
                // by distance from the start silenced everything past
                // it.
                var lo = start - A.rate * 10;
                var hi = start + count + A.rate * 10;
                for (var k in A.cache)
                {
                    var pos = A.frames[k].pos;
                    if (pos < lo || pos > hi)
                    {
                        delete A.cache[k];
                    }
                }
                resolve();
            }).catch(reject);
        });
    });
}

// A range of samples, interleaved into the reader's buffer; what the
// track does not cover is silence.
function requestAudio(A, seq)
{
    var v = views(A.ctrl);
    var start = Math.round(v.f64[1]);
    A.ptr = v.u32[4];
    A.cap = v.u32[5];
    if (!A.frames)
    {
        v.i32[7] = 0;
        v.f64[4] = -1;
        respond(A, seq);
        return;
    }
    var count = Math.floor(A.cap / (A.channels * 4));
    ensureAudio(A, start, count).then(function()
    {
        var heap = new Float32Array(buf());
        var base = A.ptr >> 2;
        heap.fill(0, base, base + count * A.channels);
        if (A.frames.length && start < A.total && start + count > 0)
        {
            var f0 = frameForPos(A, Math.max(0, start));
            var f1 = frameForPos(A, Math.min(A.total, start + count) - 1);
            for (var i = f0; i <= f1; ++i)
            {
                var c = A.cache[i];
                if (!c)
                {
                    continue;
                }
                var lo = Math.max(c.pos, start);
                var hi = Math.min(c.pos + c.len, start + count);
                for (var s = lo; s < hi; ++s)
                {
                    for (var ch = 0; ch < A.channels; ++ch)
                    {
                        heap[base + (s - start) * A.channels + ch] =
                            c.planes[ch][s - c.pos];
                    }
                }
            }
        }
        var v2 = views(A.ctrl);
        v2.i32[7] = 0;
        v2.f64[4] = start / A.rate * 1e6;
        respond(A, seq);
    }).catch(function(e)
    {
        console.error('tl::webcodecs:', String(e));
        var v2 = views(A.ctrl);
        v2.i32[7] = 0;
        v2.f64[4] = -1;
        respond(A, seq);
    });
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
    // The downloads run a little ahead of the decode; a sample's
    // bytes are dropped once they are fed, so the store only ever
    // holds the window.
    var AHEAD = 24;
    R.prefetch = function()
    {
        var last = Math.min(R.samples.length - 1, R.next + AHEAD);
        for (var i = R.next; i <= last; ++i)
        {
            if (R.store[i] || R.pending[i])
            {
                continue;
            }
            (function(i)
            {
                R.pending[i] = true;
                var s = R.samples[i];
                readRange(R.src, s.off, s.size).then(function(ab)
                {
                    delete R.pending[i];
                    R.store[i] = new Uint8Array(ab);
                    R.pump();
                }).catch(function(e)
                {
                    delete R.pending[i];
                    console.error('tl::webcodecs: fetch:', String(e));
                });
            })(i);
        }
    };
    R.pump = function()
    {
        while (R.dec &&
            'configured' === R.dec.state &&
            R.next < R.samples.length &&
            R.dec.decodeQueueSize < 8 &&
            R.queue.length < 8)
        {
            var d = R.store[R.next];
            if (!d)
            {
                break;
            }
            var s = R.samples[R.next];
            delete R.store[R.next];
            ++R.next;
            R.dec.decode(new EncodedVideoChunk({
                type: s.key ? 'key' : 'delta',
                timestamp: s.ts,
                duration: s.dur,
                data: d
            }));
        }
        R.prefetch();
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
    getSource(R.url).then(function(S)
    {
        R.src = S;
        return parseIndex(S);
    }).then(function(ix)
    {
        var track = ix.info.videoTracks[0];
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
        var desc = description(ix.file.getTrackById(track.id));
        if (desc)
        {
            R.config.description = desc;
        }
        // The composition times carry the B frame delay; the edit
        // list is what normally corrects it, so the smallest time
        // is subtracted from all of them.
        var raw = ix.file.getTrackById(track.id).samples;
        var offset = Infinity;
        for (var i = 0; i < raw.length; ++i)
        {
            offset = Math.min(offset, raw[i].cts);
        }
        for (var i = 0; i < raw.length; ++i)
        {
            var s = raw[i];
            R.samples.push({
                ts: Math.round((s.cts - offset) * 1e6 / s.timescale),
                dur: Math.round(s.duration * 1e6 / s.timescale),
                key: !!s.is_sync,
                off: s.offset,
                size: s.size
            });
        }
        if (R.samples.length)
        {
            R.frameDur = R.samples[0].dur || 42000;
        }
        // Support differs by browser -- a stream one engine decodes
        // can be refused by another, and the refusal should say so.
        var check = VideoDecoder.isConfigSupported ?
            VideoDecoder.isConfigSupported(R.config) :
            Promise.resolve({ supported: true });
        return check.then(function(res)
        {
            if (res && false === res.supported)
            {
                fail('unsupported video: ' + R.config.codec +
                    ' ' + R.w + 'x' + R.h);
                return;
            }
            R.dec = new VideoDecoder({
                output: onFrame,
                error: fail
            });
            // The decoder can want more input before its first
            // output; this is what feeds it when the queue drains.
            R.dec.ondequeue = function() { R.pump(); };
            R.dec.configure(R.config);
            var v = views(R.ctrl);
            v.i32[10] = R.w;
            v.i32[11] = R.h;
            v.i32[12] = R.samples.length;
            v.f64[7] = R.dur;
            v.f64[8] = R.frameDur;
            respond(R, 1);
            watch(R);
        });
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
        if (R.samples[i].ts > t)
        {
            break;
        }
        if (R.samples[i].key)
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
        R.queue = [];
        // Scrubbing leaves stragglers behind; anything the new decode
        // will not feed goes.
        for (var k in R.store)
        {
            if (k < key || k > key + 256)
            {
                delete R.store[k];
            }
        }
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
                if (R.adec)
                {
                    try { R.adec.close(); } catch (e) {}
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
            src: null,
            frames: null,
            cache: null,
            adec: null,
            config: null,
            batch: null,
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
            src: null,
            samples: [],
            store: {},
            pending: {},
            queue: [],
            next: 0,
            dec: null,
            config: null,
            flushed: false,
            eos: false,
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
