//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "decode.h"
#include "error.h"
#include "heif.h"
#include "image.h"
#include "jpeg.h"
#include "options.h"
#include "png.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../compress/level.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "../io/file.h"
#include "../io/fs.h"

#include <string>

// Images on files (DESIGN 325): codec::load reads a file of any of the
// module's formats, told by its signature; image::save and codec::save
// write one in the format its path's extension names
namespace sgcl::codec {
    // What save writes: each field is for the formats it names, the others
    // leave it alone. img.save("photo.jpg", {.quality = 90})
    struct save_options {
        compress::level level = detail::PngLevel;                    // PNG: 0 stores, 1 fastest, 9 smallest; 7 the default
        int quality = 85;                                            // JPEG and HEIC: 1..100
        jpeg::subsampling subsampling = jpeg::subsampling::s420;     // JPEG
    };

    // The image of the file at path, in any of the module's formats, told
    // by its first bytes as decode tells them: errc::io with io's error
    // inside when the file does not read, the decoder's error else
    inline expected<image, error> load(const string& path, const decode_options& o = {}) {
        auto bytes = io::read_file(path);
        if (!bytes) {
            return unexpected(error(bytes.error(), 0));
        }
        return decode(bytes->as_slice(), o);
    }

    namespace detail {
        // by value: a task is lazy, the caller's arguments may be gone
        // before it runs
        inline async::task<expected<image, error>> load_task(string path, decode_options o) noexcept {
            co_return co_await async::spawn_blocking([path, o] { return load(path, o); });
        }

        enum class SaveFormat : uint8_t { png, jpeg, heif, unwritten, unknown };

        // The format the path's extension names, letters of either case:
        // .png, .jpg or .jpeg, .heic or .heif; .gif, .webp and .avif are
        // formats the module reads and does not write; anything else,
        // including no extension, is unknown
        inline SaveFormat save_format(const string& path, std::string& extension) noexcept {
            const std::string p(path.data(), path.size());
            const size_t dot = p.rfind('.');
            const size_t slash = p.find_last_of("/\\");
            if (dot == std::string::npos || (slash != std::string::npos && slash > dot)) {
                extension.clear();
                return SaveFormat::unknown;
            }
            extension = p.substr(dot + 1);
            for (char& c : extension) {
                if (c >= 'A' && c <= 'Z') {
                    c = char(c - 'A' + 'a');
                }
            }
            if (extension == "png") {
                return SaveFormat::png;
            }
            if (extension == "jpg" || extension == "jpeg") {
                return SaveFormat::jpeg;
            }
            if (extension == "heic" || extension == "heif") {
                return SaveFormat::heif;
            }
            if (extension == "gif" || extension == "webp" || extension == "avif") {
                return SaveFormat::unwritten;
            }
            return SaveFormat::unknown;
        }

        // The file path + ".part" while it is written, removed unless done()
        // renamed it over path: a failure, of the stream or of the encoder
        // (an exception too), leaves neither a part nor a half file
        class PartFile {
        public:
            explicit PartFile(const string& path)
            : _part(string::concat(path, ".part")) {
            }

            PartFile(const PartFile&) = delete;
            PartFile& operator=(const PartFile&) = delete;

            ~PartFile() {
                if (_file) {
                    (void)_file->close();
                }
                if (_made && !_done) {
                    (void)io::remove(_part);
                }
            }

            expected<void, error> create() noexcept {
                auto f = io::create(_part);
                if (!f) {
                    return unexpected(error(f.error(), 0));
                }
                _file.emplace(std::move(*f));
                _made = true;
                return {};
            }

            io::file& file() noexcept {
                return *_file;
            }

            // closed, then renamed over path
            expected<void, error> done(const string& path) noexcept {
                auto closed = _file->close();
                _file.reset();
                if (!closed) {
                    return unexpected(error(closed.error(), 0));
                }
                if (auto r = io::rename(_part, path); !r) {
                    return unexpected(error(r.error(), 0));
                }
                _done = true;
                return {};
            }

        private:
            string _part;
            optional<io::file> _file;
            bool _made = false;
            bool _done = false;
        };

        inline expected<void, error> save(const image& im, const string& path, const save_options& o) {
            std::string extension;
            const SaveFormat f = save_format(path, extension);
            if (f == SaveFormat::unwritten) {
                return unexpected(error(errc::unsupported, 0, string(std::string("codec: .") + extension + " is read, not written (no encoder)")));
            }
            if (f == SaveFormat::unknown) {
                return unexpected(error(errc::unsupported, 0,
                                        string(extension.empty() ? std::string("codec: a path with no extension (.png, .jpg, .jpeg, .heic, .heif)")
                                                                 : std::string("codec: .") + extension + " is no format the module writes")));
            }
            PartFile part(path);
            if (auto c = part.create(); !c) {
                return c;
            }
            const io::writer out(part.file());
            expected<void, error> written;
            switch (f) {
                case SaveFormat::png:
                    written = png::encode(im, out, png::options{.level = o.level});
                    break;
                case SaveFormat::jpeg:
                    written = jpeg::encode(im, out, jpeg::options{.quality = o.quality, .subsampling = o.subsampling});
                    break;
                default:
                    written = heif::encode(im, out, heif::options{.quality = o.quality});
                    break;
            }
            if (!written) {
                return written;
            }
            return part.done(path);
        }

        inline async::task<expected<void, error>> save_task(image im, string path, save_options o) noexcept {

            co_return co_await async::spawn_blocking([im, path, o] { return detail::save(im, path, o); });
        }
    }

    // The same, run on the blocking pool for a task
    inline async::task<expected<image, error>> async_load(const string& path, const decode_options& o = {}) noexcept {
        return detail::load_task(path, o);
    }

    // The image into the file at path, in the format its extension names
    // (.png, .jpg or .jpeg, and .heic or .heif where the system writes
    // HEIC): errc::unsupported for .gif, .webp, .avif (read, not written)
    // and any other extension. Written as path + ".part" and renamed over
    // path when whole; a failure leaves no part and path as it was
    inline expected<void, error> save(const image& im, const string& path, const save_options& o = {}) {
        return detail::save(im, path, o);
    }

    inline async::task<expected<void, error>> async_save(const image& im, const string& path, const save_options& o = {}) noexcept {

        return detail::save_task(im, path, o);
    }

    // image's own (declared in image.h)
    inline expected<void, error> image::save(const string& path) const {
        return detail::save(*this, path, save_options{});
    }

    inline expected<void, error> image::save(const string& path, const save_options& o) const {
        return detail::save(*this, path, o);
    }

    inline async::task<expected<void, error>> image::async_save(const string& path) const noexcept {
        return detail::save_task(*this, path, save_options{});
    }

    inline async::task<expected<void, error>> image::async_save(const string& path, const save_options& o) const noexcept {
        return detail::save_task(*this, path, o);
    }
}
