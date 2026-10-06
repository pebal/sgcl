//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bmp.h"
#include "error.h"
#include "image.h"
#include "options.h"
#include "png.h"
#include "detail/input.h"
#include "detail/output.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"
#include "../io/stream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sgcl::codec {
    namespace detail {
        // ico::options, outside the class (a nested struct with member
        // initializers cannot be a default argument inside its class)
        struct IcoOptions {
            bool cursor = false;       // CUR rather than ICO: each entry with the hotspot below
            uint16_t hotspot_x = 0;    // the cursor's hot pixel, from the left
            uint16_t hotspot_y = 0;    // and from the top
        };

        // An entry of an icon's directory
        struct IcoEntry {
            uint32_t width;
            uint32_t height;
            uint32_t bits;
            uint32_t size;
            uint32_t offset;
        };

        inline error ico_fail(errc code, uint64_t at, const char* what) noexcept {
            return error(code, at, string(what));
        }

        // The directory of an ICO or CUR file (6 bytes, then 16 an entry):
        // false with the error; every entry inside the data
        inline bool ico_directory(const slice<const byte>& data, std::vector<IcoEntry>& entries, error& err) noexcept {
            const auto* p = reinterpret_cast<const uint8_t*>(data.data());
            const size_t n = data.size();
            auto le16 = [&](size_t at) { return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8; };
            auto le32 = [&](size_t at) { return le16(at) | le16(at + 2) << 16; };
            if (n < 6) {
                err = ico_fail(errc::unexpected_end, n, "ico: the data ends in the header");
                return false;
            }
            if (le16(0) != 0 || (le16(2) != 1 && le16(2) != 2)) {
                err = ico_fail(errc::corrupt, 0, "ico: not an ICO or CUR header");
                return false;
            }
            const uint32_t count = le16(4);
            if (count == 0) {
                err = ico_fail(errc::corrupt, 4, "ico: a directory of no entry");
                return false;
            }
            if (n < 6 + size_t(count) * 16) {
                err = ico_fail(errc::unexpected_end, n, "ico: the data ends in the directory");
                return false;
            }
            entries.clear();
            for (uint32_t i = 0; i < count; ++i) {
                const size_t at = 6 + size_t(i) * 16;
                IcoEntry e;
                e.width = p[at] ? p[at] : 256;
                e.height = p[at + 1] ? p[at + 1] : 256;
                e.bits = le16(2) == 1 ? le16(at + 6) : 32;   // CUR: the hotspot there
                e.size = le32(at + 8);
                e.offset = le32(at + 12);
                if (uint64_t(e.offset) + e.size > n || e.size == 0) {
                    err = ico_fail(errc::corrupt, at + 8, "ico: an entry outside the data");
                    return false;
                }
                entries.push_back(e);
            }
            return true;
        }

        // One entry's image: PNG when it begins with PNG's signature, else a
        // BMP without its file header (its height doubled, an AND mask
        // after its pixels)
        inline expected<image, error> ico_entry(const slice<const byte>& data, const IcoEntry& e, const decode_options& o) noexcept {
            const slice<const byte> body(data.data() + e.offset, e.size);
            const auto* p = reinterpret_cast<const uint8_t*>(body.data());
            if (e.size >= 8 && std::memcmp(p, "\x89PNG\r\n\x1a\n", 8) == 0) {
                return png::decode(body, o);
            }
            MemoryInput in(body);
            return BmpDecoder<MemoryInput>(in, o, true).run();
        }

        // The file into a sink: each image an entry, PNG inside for a side
        // of 256, a 32-bit BMP with its AND mask below
        template<class Sink>
        bool ico_encode(const slice<const image>& images, const IcoOptions& o, Sink& sink) noexcept(NothrowSink<Sink>) {
            if (images.size() == 0 || images.size() > 65535) {
                sink.failure = error(errc::invalid_argument, 0, "ico: no image, or more than a directory holds");
                return false;
            }
            for (const image& im : images) {
                if (im.width() > 256 || im.height() > 256) {
                    sink.failure = error(errc::invalid_argument, 0, "ico: a side past 256 pixels, more than an entry holds");
                    return false;
                }
            }
            std::vector<std::vector<uint8_t>> parts;
            for (const image& im : images) {
                std::vector<uint8_t> part;
                if (im.width() == 256 || im.height() == 256) {
                    auto f = png::encode(im);
                    const auto* b = reinterpret_cast<const uint8_t*>(f->data());
                    part.assign(b, b + f->size());
                } else {
                    vector<byte> v;
                    VectorSink vs{v, nullopt};
                    bmp_encode(im, vs, true);
                    const auto* b = reinterpret_cast<const uint8_t*>(v.data());
                    part.assign(b, b + v.size());
                }
                parts.push_back(std::move(part));
            }
            std::vector<uint8_t> head(6 + 16 * parts.size(), 0);
            head[2] = o.cursor ? 2 : 1;
            head[4] = uint8_t(parts.size());
            head[5] = uint8_t(parts.size() >> 8);
            uint64_t offset = head.size();
            for (size_t i = 0; i < parts.size(); ++i) {
                uint8_t* e = head.data() + 6 + 16 * i;
                const image& im = images[i];
                e[0] = uint8_t(im.width() == 256 ? 0 : im.width());
                e[1] = uint8_t(im.height() == 256 ? 0 : im.height());
                if (o.cursor) {
                    e[4] = uint8_t(o.hotspot_x);
                    e[5] = uint8_t(o.hotspot_x >> 8);
                    e[6] = uint8_t(o.hotspot_y);
                    e[7] = uint8_t(o.hotspot_y >> 8);
                } else {
                    e[4] = 1;    // planes
                    e[6] = 32;   // bits
                }
                const uint32_t size = uint32_t(parts[i].size());
                for (int k = 0; k < 4; ++k) {
                    e[8 + k] = uint8_t(size >> (8 * k));
                    e[12 + k] = uint8_t(uint32_t(offset) >> (8 * k));
                }
                offset += size;
            }
            if (!sink.put(head.data(), head.size())) {
                return false;
            }
            for (const auto& part : parts) {
                if (!sink.put(part.data(), part.size())) {
                    return false;
                }
            }
            return true;
        }
    }

    // ICO and CUR, Windows' icons and cursors: a directory of entries, each
    // a PNG or a BMP with an AND mask. decode gives the largest entry (the
    // most pixels, then the most bits), decode_all every entry in the
    // directory's order; a stream is read to its end first (the entries lie
    // where the directory says). encode writes one image or several, PNG
    // inside for a side of 256, a 32-bit BMP and its mask below.
    class ico {
    public:
        using options = detail::IcoOptions;

        static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            std::vector<detail::IcoEntry> entries;
            error err;
            if (!detail::ico_directory(data, entries, err)) {
                return unexpected(err);
            }
            size_t best = 0;
            for (size_t i = 1; i < entries.size(); ++i) {
                const uint64_t a = uint64_t(entries[i].width) * entries[i].height, b = uint64_t(entries[best].width) * entries[best].height;
                if (a > b || (a == b && entries[i].bits > entries[best].bits)) {
                    best = i;
                }
            }
            return detail::ico_entry(data, entries[best], o);
        }

        static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            vector<byte> all;
            if (auto e = _read_all(in, o, all); !e) {
                return unexpected(e.error());
            }
            return decode(all.as_slice(), o);
        }

        // Every entry, in the directory's order
        static expected<vector<image>, error> decode_all(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            std::vector<detail::IcoEntry> entries;
            error err;
            if (!detail::ico_directory(data, entries, err)) {
                return unexpected(err);
            }
            vector<image> out;
            for (const auto& e : entries) {
                auto im = detail::ico_entry(data, e, o);
                if (!im) {
                    return unexpected(im.error());
                }
                out.push_back(*im);
            }
            return out;
        }

        static expected<vector<image>, error> decode_all(const io::reader& in, const decode_options& o = {}) {
            vector<byte> all;
            if (auto e = _read_all(in, o, all); !e) {
                return unexpected(e.error());
            }
            return decode_all(all.as_slice(), o);
        }

        // The file of one image, or of several (one entry each), as bytes:
        // errc::invalid_argument for a side past 256 or no image
        static expected<vector<byte>, error> encode(const image& im, const options& o = {}) noexcept {
            return encode(slice<const image>(&im, 1), o);
        }

        static expected<vector<byte>, error> encode(const slice<const image>& images, const options& o = {}) noexcept {
            vector<byte> out;
            detail::VectorSink sink{out, nullopt};
            if (!detail::ico_encode(images, o, sink)) {
                return unexpected(*sink.failure);
            }
            return out;
        }

        static expected<void, error> encode(const image& im, const io::writer& out, const options& o = {}) {
            return encode(slice<const image>(&im, 1), out, o);
        }

        static expected<void, error> encode(const slice<const image>& images, const io::writer& out, const options& o = {}) {
            detail::WriterSink sink{out, 0, nullopt};
            if (!detail::ico_encode(images, o, sink)) {
                return unexpected(*sink.failure);
            }
            return {};
        }

    private:
        // A stream to its end: at most 4 bytes a pixel of limits.max_pixels
        // (and 64 KB), errc::too_large past it
        static expected<void, error> _read_all(const io::reader& in, const decode_options& o, vector<byte>& all) {
            detail::ReaderInput source(in);
            const uint64_t most = std::max<uint64_t>(o.limits.max_pixels * 4 + 65536, 1u << 20);
            for (;;) {
                const uint8_t* p;
                size_t got;
                if (!source.peek(65536, p, got)) {
                    return unexpected(*source.failure);
                }
                if (got == 0) {
                    return {};
                }
                if (all.size() + got > most) {
                    return unexpected(error(errc::too_large, all.size()));
                }
                all.insert(all.end(), reinterpret_cast<const byte*>(p), reinterpret_cast<const byte*>(p) + got);
                source.consume(got);
            }
        }
    };
}
