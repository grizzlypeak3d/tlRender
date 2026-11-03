// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/C/Engine.h>

#include <tlRender/Core/URL.h>

#include <ftk/Core/Path.h>
#include <ftk/Core/String.h>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>
#include <opentimelineio/imageSequenceReference.h>
#include <opentimelineio/timeline.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <vector>

// Written here rather than with OTIO's bundle writer, which puts every
// file in one folder and gives up when two share a name -- every shot's
// frame.0001.exr -- and stops at the first file that is not there.

namespace
{
    namespace fs = std::filesystem;

    fs::path toPath(const std::string& value)
    {
        return ftk::toFileSystem(value);
    }

    std::string fromPath(const fs::path& value)
    {
        return ftk::fromFileSystem(value);
    }

    //! The path in a URL: a plain path, or a file URL. Nothing for any
    //! other kind.
    std::optional<std::string> fileFromURL(const std::string& url)
    {
        const std::string scheme = ftk::toLower(tl::getURLScheme(url));
        std::string out;
        if (scheme.empty())
        {
            out = url;
        }
        else if ("file://" == scheme)
        {
            out = url.substr(scheme.size());
            // A host before the path: file://localhost/...
            if (!out.empty() && out[0] != '/')
            {
                const size_t i = out.find('/');
                out = i != std::string::npos ? out.substr(i) : std::string();
            }
        }
        else
        {
            return std::nullopt;
        }
        out = tl::decodeURL(out);
        if (out.empty())
            return std::nullopt;
        return out;
    }

    //! A reference's path in pieces, split at either separator, since the
    //! editor may have been on Windows.
    std::vector<std::string> pieces(const std::string& path)
    {
        std::vector<std::string> out;
        std::string piece;
        for (char c : path)
        {
            if ('/' == c || '\\' == c)
            {
                if (!piece.empty())
                    out.push_back(piece);
                piece.clear();
            }
            else
            {
                piece.push_back(c);
            }
        }
        if (!piece.empty())
            out.push_back(piece);
        return out;
    }

    bool isRelative(const std::string& path)
    {
        if (path.empty())
            return false;
        if ('/' == path[0] || '\\' == path[0])
            return false;
        // A drive letter.
        if (path.size() > 1 && ':' == path[1])
            return false;
        return true;
    }

    bool isFile(const fs::path& path)
    {
        std::error_code ec;
        return fs::is_regular_file(path, ec);
    }

    //! Every file under a folder, by name in lower case.
    using Index = std::map<std::string, std::vector<fs::path> >;

    struct Cancelled {};

    //! Every file under a folder. The caller is asked now and then whether
    //! to go on, as a big folder on a share takes a while.
    Index indexFolder(const fs::path& folder, tl_bundle_progress callback, void* user)
    {
        Index out;
        std::error_code ec;
        fs::recursive_directory_iterator i(
            folder, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        for (size_t n = 0; !ec && i != end; i.increment(ec), ++n)
        {
            if (callback && 0 == n % 256 && !callback(-1.0, user))
                throw Cancelled();
            std::error_code ec2;
            if (i->is_regular_file(ec2))
            {
                out[ftk::toLower(fromPath(i->path().filename()))].push_back(i->path());
            }
        }
        return out;
    }

    //! How many of the last pieces of a reference a file's path shares,
    //! ignoring case: the file name, then its folder, and up.
    size_t sharedEnd(const std::vector<std::string>& ref, const fs::path& file)
    {
        std::vector<std::string> other;
        for (const auto& i : file)
        {
            other.push_back(fromPath(i));
        }
        size_t out = 0;
        while (out < ref.size() && out < other.size() &&
            ftk::toLower(ref[ref.size() - 1 - out]) ==
            ftk::toLower(other[other.size() - 1 - out]))
        {
            ++out;
        }
        return out;
    }

    //! Find a file: where the reference says, and then under the media
    //! folder.
    std::optional<fs::path> findFile(
        const std::string& path,
        const fs::path& otioDir,
        const Index& index)
    {
        if (isRelative(path))
        {
            const fs::path p = otioDir / toPath(path);
            if (isFile(p))
                return p;
        }
        else if (isFile(toPath(path)))
        {
            return toPath(path);
        }
        const auto ref = pieces(path);
        if (ref.empty())
            return std::nullopt;
        const auto i = index.find(ftk::toLower(ref.back()));
        if (i == index.end())
            return std::nullopt;
        std::optional<fs::path> out;
        size_t best = 0;
        for (const auto& file : i->second)
        {
            const size_t n = sharedEnd(ref, file);
            if (n > best)
            {
                best = n;
                out = file;
            }
        }
        return out;
    }

    //! A URL for a path in the bundle: as it is, but for a percent, which
    //! reading it would take for an escape.
    std::string toURL(const std::string& path)
    {
        std::string out;
        for (char c : path)
        {
            if ('%' == c)
                out += "%25";
            else
                out.push_back(c);
        }
        return out;
    }

    //! Names in the bundle's media folder, one for each source.
    class Names
    {
    public:
        //! The name for a source: its own, or with a number when another
        //! source has it.
        std::string get(const fs::path& source, const std::string& name)
        {
            const auto i = _bySource.find(source);
            if (i != _bySource.end())
                return i->second;
            const fs::path p = toPath(name);
            const std::string stem = fromPath(p.stem());
            const std::string ext = fromPath(p.extension());
            std::string out = name;
            for (int n = 2; _taken.count(ftk::toLower(out)); ++n)
            {
                out = stem + " " + std::to_string(n) + ext;
            }
            _taken.insert(ftk::toLower(out));
            _bySource[source] = out;
            return out;
        }

    private:
        std::map<fs::path, std::string> _bySource;
        std::set<std::string> _taken;
    };

    void copyString(const std::string& value, char* out, size_t size)
    {
        std::strncpy(out, value.c_str(), size - 1);
        out[size - 1] = 0;
    }

    //! A file's size, if it is a file: one round trip on a share, where
    //! a check and then the size would be two.
    std::optional<uint64_t> fileSize(const fs::path& path)
    {
        std::error_code ec;
        const uint64_t size = fs::file_size(path, ec);
        if (ec)
            return std::nullopt;
        return size;
    }

    //! A file to copy into the bundle.
    struct Copy
    {
        std::string name;
        uint64_t size = 0;
    };

    //! Counts what has been copied, and tells the caller.
    class Progress
    {
    public:
        Progress(uint64_t total, tl_bundle_progress callback, void* user) :
            _total(total),
            _callback(callback),
            _user(user)
        {
            _report();
        }

        void add(uint64_t bytes)
        {
            _done += bytes;
            _report();
        }

    private:
        void _report()
        {
            if (_callback &&
                !_callback(_total > 0 ? static_cast<double>(_done) / _total : 1.0, _user))
            {
                throw Cancelled();
            }
        }

        uint64_t _total = 0;
        uint64_t _done = 0;
        tl_bundle_progress _callback = nullptr;
        void* _user = nullptr;
    };

    //! Copied a piece at a time rather than whole, so that one long movie
    //! still shows progress, and can be stopped.
    void copyFile(const fs::path& from, const fs::path& to, Progress& progress)
    {
        std::ifstream in(from, std::ios::binary);
        if (!in)
            throw std::runtime_error("Cannot read \"" + fromPath(from.filename()) + "\"");
        std::ofstream out(to, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot write \"" + fromPath(to.filename()) + "\"");
        std::vector<char> buffer(8 * 1024 * 1024);
        while (in)
        {
            in.read(buffer.data(), buffer.size());
            const std::streamsize n = in.gcount();
            if (n <= 0)
                break;
            out.write(buffer.data(), n);
            if (!out)
                throw std::runtime_error("Cannot write \"" + fromPath(to.filename()) + "\"");
            progress.add(static_cast<uint64_t>(n));
        }
        if (in.bad())
            throw std::runtime_error("Cannot read \"" + fromPath(from.filename()) + "\"");
    }
}

namespace
{
    //! A file's path from the media folder, if it is under it: what a
    //! relinked timeline names it by, the folder's own place being one
    //! that can change between mounts.
    std::optional<std::string> underFolder(const fs::path& file, const fs::path& folder)
    {
        std::error_code ec;
        const fs::path f = fs::weakly_canonical(file, ec);
        const fs::path root = fs::weakly_canonical(folder, ec);
        if (root.empty())
            return std::nullopt;
        auto i = f.begin();
        for (auto j = root.begin(); j != root.end(); ++j, ++i)
        {
            if (j->empty())
                continue;
            if (i == f.end() || *i != *j)
                return std::nullopt;
        }
        fs::path rest;
        for (; i != f.end(); ++i)
        {
            rest /= *i;
        }
        if (rest.empty())
            return std::nullopt;
        // Forward slashes, as a URL has them.
        return ftk::fromFileSystemGeneric(rest);
    }
}

//! A bundle, or with relink a timeline alone, its references pointing at
//! the media where it was found.
static int writeBundle(
    const char* otio,
    const char* mediaFolder,
    const char* out,
    tl_bundle_progress progressCallback,
    void* progressUser,
    tl_bundle_result* result,
    bool relink);

int tl_bundle_write_otiod(
    const char* otio,
    const char* mediaFolder,
    const char* out,
    tl_bundle_progress progressCallback,
    void* progressUser,
    tl_bundle_result* result)
{
    return writeBundle(otio, mediaFolder, out, progressCallback, progressUser, result, false);
}

int tl_bundle_relink_otio(
    const char* otio,
    const char* mediaFolder,
    const char* out,
    tl_bundle_progress progressCallback,
    void* progressUser,
    tl_bundle_result* result)
{
    return writeBundle(otio, mediaFolder, out, progressCallback, progressUser, result, true);
}

static int writeBundle(
    const char* otio,
    const char* mediaFolder,
    const char* out,
    tl_bundle_progress progressCallback,
    void* progressUser,
    tl_bundle_result* result,
    bool relink)
{
    tl_bundle_result r;
    std::memset(&r, 0, sizeof(r));
    const auto finish = [&r, result](int value)
    {
        if (result)
            *result = r;
        return value;
    };
    if (!otio || !out)
    {
        copyString("No file given", r.error, sizeof(r.error));
        return finish(0);
    }
    const fs::path otioPath = toPath(otio);
    const fs::path outPath = toPath(out);
    std::error_code ec;
    if (fs::exists(outPath, ec))
    {
        copyString(relink ? "The timeline already exists" : "The bundle already exists", r.error, sizeof(r.error));
        return finish(0);
    }

    OTIO_NS::ErrorStatus error;
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> timeline(
        dynamic_cast<OTIO_NS::Timeline*>(
            OTIO_NS::Timeline::from_json_file(otio, &error)));
    if (!timeline || OTIO_NS::is_error(error))
    {
        copyString(
            "Cannot read the timeline" +
            (error.details.empty() ? std::string() : ": " + error.details),
            r.error, sizeof(r.error));
        return finish(0);
    }

    const fs::path otioDir = otioPath.parent_path();
    const fs::path folderPath = mediaFolder && mediaFolder[0] ? toPath(mediaFolder) : fs::path();
    // Where a relinked reference points: under the media folder, by its
    // path from there; outside it, where it is.
    const auto linked = [&folderPath](const fs::path& file)
    {
        if (const auto rest = underFolder(file, folderPath))
            return *rest;
        return fromPath(file);
    };
    Index index;
    try
    {
        if (mediaFolder && mediaFolder[0])
            index = indexFolder(toPath(mediaFolder), progressCallback, progressUser);
    }
    catch (const Cancelled&)
    {
        copyString("Cancelled", r.error, sizeof(r.error));
        return finish(0);
    }

    // What to copy, from where to where in the bundle; and what was not
    // found, by the reference's own path.
    std::map<fs::path, Copy> copies;
    std::set<std::string> found;
    std::set<std::string> missing;
    Names files;
    Names folders;
    for (const auto& clip : timeline->find_clips())
    {
        for (const auto& i : clip->media_references())
        {
            if (auto ext = dynamic_cast<OTIO_NS::ExternalReference*>(i.second))
            {
                const auto path = fileFromURL(ext->target_url());
                if (!path)
                    continue;
                const auto file = findFile(*path, otioDir, index);
                const auto size = file ? fileSize(*file) : std::nullopt;
                if (file && size && relink)
                {
                    ext->set_target_url(toURL(linked(*file)));
                    found.insert(*path);
                }
                else if (file && size)
                {
                    const std::string name = "media/" +
                        files.get(*file, fromPath(file->filename()));
                    copies[*file] = Copy{ name, *size };
                    ext->set_target_url(toURL(name));
                    found.insert(*path);
                }
                else
                {
                    missing.insert(*path);
                }
            }
            else if (auto seq = dynamic_cast<OTIO_NS::ImageSequenceReference*>(i.second))
            {
                // Found by the first frame; the rest are beside it. A
                // sequence keeps a folder of its own, under the name of
                // the folder it came from, as they are named alike.
                OTIO_NS::ErrorStatus seqError;
                const int count = seq->number_of_images_in_sequence();
                const auto first = fileFromURL(
                    seq->target_url_for_image_number(0, &seqError));
                if (!first || OTIO_NS::is_error(seqError) || count <= 0)
                    continue;
                const auto file = findFile(*first, otioDir, index);
                if (!file)
                {
                    missing.insert(*first);
                    continue;
                }
                const fs::path sourceDir = file->parent_path();
                if (relink)
                {
                    std::string base = linked(sourceDir);
                    if (!base.empty() && base.back() != '/')
                        base += '/';
                    seq->set_target_url_base(toURL(base));
                    found.insert(*first);
                    continue;
                }
                std::string folder = fromPath(sourceDir.filename());
                if (folder.empty())
                    folder = "sequence";
                const std::string base = "media/" + folders.get(sourceDir, folder) + "/";
                for (int frame = 0; frame < count; ++frame)
                {
                    OTIO_NS::ErrorStatus frameError;
                    const auto url = seq->target_url_for_image_number(frame, &frameError);
                    if (OTIO_NS::is_error(frameError))
                        break;
                    const auto framePath = fileFromURL(url);
                    if (!framePath)
                        continue;
                    const auto framePieces = pieces(*framePath);
                    if (framePieces.empty())
                        continue;
                    const fs::path source = sourceDir / toPath(framePieces.back());
                    if (const auto size = fileSize(source))
                    {
                        copies[source] = Copy{ base + framePieces.back(), *size };
                    }
                }
                seq->set_target_url_base(toURL(base));
                found.insert(*first);
            }
        }
    }
    r.media = static_cast<int>(found.size() + missing.size());
    r.missing = static_cast<int>(missing.size());
    std::string names;
    for (const auto& i : missing)
    {
        const auto p = pieces(i);
        const std::string line = (p.empty() ? i : p.back()) + "\n";
        if (names.size() + line.size() >= sizeof(r.missingNames))
            break;
        names += line;
    }
    copyString(names, r.missingNames, sizeof(r.missingNames));

    // Written, and removed again if any of it cannot be, or it is stopped.
    std::string failure;
    if (relink)
    {
        try
        {
            fs::create_directories(outPath.parent_path());
            OTIO_NS::ErrorStatus writeError;
            if (!timeline->to_json_file(fromPath(outPath), &writeError) ||
                OTIO_NS::is_error(writeError))
            {
                throw std::runtime_error("Cannot write the timeline: " + writeError.details);
            }
        }
        catch (const std::exception& e)
        {
            fs::remove(outPath, ec);
            copyString(e.what(), r.error, sizeof(r.error));
            return finish(0);
        }
        return finish(1);
    }
    try
    {
        uint64_t total = 0;
        for (const auto& i : copies)
        {
            total += i.second.size;
        }
        Progress progress(total, progressCallback, progressUser);
        fs::create_directories(outPath / "media");
        for (const auto& i : copies)
        {
            const fs::path to = outPath / toPath(i.second.name);
            fs::create_directories(to.parent_path());
            copyFile(i.first, to, progress);
        }
        {
            std::ofstream version(outPath / "version.txt");
            version << "1.0.0";
            if (!version)
                throw std::runtime_error("Cannot write the version");
        }
        OTIO_NS::ErrorStatus writeError;
        if (!timeline->to_json_file(fromPath(outPath / "content.otio"), &writeError) ||
            OTIO_NS::is_error(writeError))
        {
            throw std::runtime_error("Cannot write the timeline: " + writeError.details);
        }
    }
    catch (const Cancelled&)
    {
        failure = "Cancelled";
    }
    catch (const std::exception& e)
    {
        failure = e.what();
    }
    if (!failure.empty())
    {
        fs::remove_all(outPath, ec);
        copyString(failure, r.error, sizeof(r.error));
        return finish(0);
    }
    return finish(1);
}
