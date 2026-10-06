//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "decode.h"
#include "error.h"
#include "gif.h"
#include "heif.h"
#include "image.h"
#include "jpeg.h"
#include "options.h"
#include "png.h"
#include "webp.h"
#include "bmp.h"
#include "ico.h"
#include "pnm.h"
#include "qoi.h"
#include "tiff.h"
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
        int quality = 85;                                            // JPEG, HEIC and WebP: 1..100 (WebP's lossless effort too)
        jpeg::subsampling subsampling = jpeg::subsampling::s420;     // JPEG
        bool lossless = false;                                       // WebP: VP8L, every pixel as it is
    };

    // The image of the file at path, in any of the module's formats, told
    // by its first bytes as decode tells them: errc::io with io's error
    // inside when the file does not read, the decoder's error else
    SGCL_INLINE_HOT expected<image, error> load(const string& path, const decode_options& o = {}) {
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

        enum class SaveFormat : uint8_t { png, jpeg, gif, webp, heif, bmp, tiff, ico, cur, qoi, pbm, pgm, ppm, pam, pnm, unwritten, unknown };

        // The format the path's extension names, letters of either case:
        // .png, .jpg or .jpeg, .gif, .webp, .heic or .heif, .bmp, .tif or
        // .tiff, .ico, .cur, .qoi, .pbm, .pgm, .ppm, .pam, .pnm; .avif and .jxl
        // are formats the module reads and does not write; anything else,
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
            if (extension == "gif") {
                return SaveFormat::gif;
            }
            if (extension == "webp") {
                return SaveFormat::webp;
            }
            struct Named {
                const char* name;
                SaveFormat format;
            };
            static constexpr Named more[] = {{"bmp", SaveFormat::bmp}, {"tif", SaveFormat::tiff}, {"tiff", SaveFormat::tiff}, {"ico", SaveFormat::ico},
                                             {"cur", SaveFormat::cur}, {"qoi", SaveFormat::qoi}, {"pbm", SaveFormat::pbm}, {"pgm", SaveFormat::pgm},
                                             {"ppm", SaveFormat::ppm}, {"pam", SaveFormat::pam}, {"pnm", SaveFormat::pnm}};
            for (const Named& m : more) {
                if (extension == m.name) {
                    return m.format;
                }
            }
            if (extension == "heic" || extension == "heif") {
                return SaveFormat::heif;
            }
            if (extension == "avif" || extension == "jxl") {
                return SaveFormat::unwritten;
            }
            return SaveFormat::unknown;
        }

        // The file path + ".part" while it is written, removed unless done()
        // renamed it over path: a failure, of the stream or of the encoder
        // (an exception too), leaves neither a part nor a half file
        class PartFile {
        public:
            SGCL_INLINE_HOT explicit PartFile(const string& path)
            : _part(string::concat(path, ".part")) {
            }

            PartFile(const PartFile&) = delete;
            PartFile& operator=(const PartFile&) = delete;

            SGCL_INLINE_HOT ~PartFile() {
                if (_file) {
                    (void)_file->close();
                }
                if (_made && !_done) {
                    (void)io::remove(_part);
                }
            }

            SGCL_INLINE_HOT expected<void, error> create() noexcept {
                auto f = io::create(_part);
                if (!f) {
                    return unexpected(error(f.error(), 0));
                }
                _file.emplace(std::move(*f));
                _made = true;
                return {};
            }

            SGCL_INLINE_HOT io::file& file() noexcept {
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
                                        string(extension.empty() ? std::string("codec: a path with no extension (.png, .jpg, .jpeg, .gif, .webp, .heic, .heif, .bmp, .tiff, .ico, .qoi, .pnm...)")
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
                case SaveFormat::gif:
                    written = gif::encode(im, out);
                    break;
                case SaveFormat::webp:
                    written = webp::encode(im, out, webp::options{.lossless = o.lossless, .quality = o.quality});
                    break;
                case SaveFormat::bmp:
                    written = bmp::encode(im, out);
                    break;
                case SaveFormat::tiff:
                    written = tiff::encode(im, out);
                    break;
                case SaveFormat::ico:
                case SaveFormat::cur:
                    written = ico::encode(im, out, ico::options{.cursor = f == SaveFormat::cur});
                    break;
                case SaveFormat::qoi:
                    written = qoi::encode(im, out);
                    break;
                case SaveFormat::pbm:
                case SaveFormat::pgm:
                case SaveFormat::ppm:
                case SaveFormat::pam:
                case SaveFormat::pnm: {
                    const pnm::kind k = f == SaveFormat::pbm ? pnm::kind::pbm : f == SaveFormat::pgm ? pnm::kind::pgm : f == SaveFormat::ppm ? pnm::kind::ppm
                                        : f == SaveFormat::pam                                       ? pnm::kind::pam
                                                                                                     : pnm::kind::automatic;
                    written = pnm::encode(im, out, pnm::options{.kind = k});
                    break;
                }
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
    SGCL_INLINE_HOT async::task<expected<image, error>> async_load(const string& path, const decode_options& o = {}) noexcept {
        return detail::load_task(path, o);
    }

    // The image into the file at path, in the format its extension names
    // (.png, .jpg or .jpeg, .gif, .webp, .bmp, .tif or .tiff, .ico, .cur,
    // .qoi, .pbm, .pgm, .ppm, .pam, .pnm, and .heic or .heif where the
    // system writes HEIC): errc::unsupported for .avif (read, not written)
    // and any other extension. Written as path + ".part" and renamed over
    // path when whole; a failure leaves no part and path as it was
    SGCL_INLINE_HOT expected<void, error> save(const image& im, const string& path, const save_options& o = {}) {
        return detail::save(im, path, o);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> async_save(const image& im, const string& path, const save_options& o = {}) noexcept {

        return detail::save_task(im, path, o);
    }

    // image's own (declared in image.h)
    SGCL_INLINE_HOT expected<void, error> image::save(const string& path) const {
        return detail::save(*this, path, save_options{});
    }

    SGCL_INLINE_HOT expected<void, error> image::save(const string& path, const save_options& o) const {
        return detail::save(*this, path, o);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> image::async_save(const string& path) const noexcept {
        return detail::save_task(*this, path, save_options{});
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> image::async_save(const string& path, const save_options& o) const noexcept {
        return detail::save_task(*this, path, o);
    }
}
