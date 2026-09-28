//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "stream.h"
#include "../../hash/crc32.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace sgcl::compress::detail {
    // The header of a 7z archive (7zFormat.txt of the LZMA SDK). The
    // signature header (32 bytes: the magic, the version, a CRC-32 and the
    // place, size and CRC-32 of the header) at the start; the header at the
    // end, plain (kHeader) or itself packed (kEncodedHeader: the streams of
    // a folder whose output is the plain header). The header describes the
    // packed streams, the folders — each a graph of coders joined by bind
    // pairs, fed by some of the packed streams, whose output is split into
    // the files' data (substreams) — and the files: names in UTF-16LE,
    // which have no stream (directories, empty files, anti-items), times,
    // attributes. Every count is checked against the bytes left before
    // anything of its size is made. The internal tables are plain memory.
    namespace sevenzip_format {
        constexpr uint8_t Signature[6] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
        constexpr size_t SignatureSize = 32;
        constexpr uint32_t MaxCoders = 32;
        constexpr uint32_t MaxStreams = 64;

        enum Id : uint8_t {
            kEnd = 0x00, kHeader = 0x01, kArchiveProperties = 0x02, kAdditionalStreamsInfo = 0x03, kMainStreamsInfo = 0x04,
            kFilesInfo = 0x05, kPackInfo = 0x06, kUnpackInfo = 0x07, kSubStreamsInfo = 0x08, kSize = 0x09, kCRC = 0x0A,
            kFolder = 0x0B, kCodersUnpackSize = 0x0C, kNumUnpackStream = 0x0D, kEmptyStream = 0x0E, kEmptyFile = 0x0F,
            kAnti = 0x10, kName = 0x11, kCTime = 0x12, kATime = 0x13, kMTime = 0x14, kWinAttributes = 0x15, kComment = 0x16,
            kEncodedHeader = 0x17, kStartPos = 0x18, kDummy = 0x19
        };

        // The methods, as their IDs read big-endian
        constexpr uint64_t Copy = 0x00;
        constexpr uint64_t Delta = 0x03;
        constexpr uint64_t Arm64 = 0x0A;
        constexpr uint64_t Riscv = 0x0B;
        constexpr uint64_t Lzma2 = 0x21;
        constexpr uint64_t Lzma = 0x030101;
        constexpr uint64_t X86 = 0x03030103;
        constexpr uint64_t Bcj2 = 0x0303011B;
        constexpr uint64_t PowerPC = 0x03030205;
        constexpr uint64_t Ia64 = 0x03030401;
        constexpr uint64_t Arm = 0x03030501;
        constexpr uint64_t ArmThumb = 0x03030701;
        constexpr uint64_t Sparc = 0x03030805;
        constexpr uint64_t Ppmd = 0x030401;
        constexpr uint64_t Deflate = 0x040108;
        constexpr uint64_t Deflate64 = 0x040109;
        constexpr uint64_t Bzip2 = 0x040202;
        constexpr uint64_t Aes = 0x06F10701;

        struct Coder {
            uint64_t method = 0;
            std::vector<uint8_t> props;
            uint32_t inputs = 1;       // packed-side streams (BCJ2: 4); one output always
            uint32_t first_input = 0;  // the folder's index of its first input
        };

        struct Folder {
            std::vector<Coder> coders;
            std::vector<std::pair<uint32_t, uint32_t>> binds;   // (input, coder whose output feeds it)
            std::vector<uint32_t> packed;                        // inputs fed by packed streams, in the order of the streams
            std::vector<uint64_t> sizes;                         // each coder's output
            uint32_t main = 0;                                   // the coder whose output is the folder's
            uint32_t inputs = 0;
            bool has_crc = false;
            uint32_t crc = 0;
            size_t first_pack = 0;                               // its first packed stream
            uint64_t substreams = 1;

            uint64_t size() const noexcept {
                return sizes[main];
            }

            bool encrypted() const noexcept {
                for (auto& c : coders) {
                    if (c.method == Aes) {
                        return true;
                    }
                }
                return false;
            }
        };

        struct Streams {
            uint64_t pack_pos = 0;
            std::vector<uint64_t> pack_sizes;
            std::vector<uint64_t> pack_offsets;   // from the archive's start
            std::vector<Folder> folders;
            std::vector<uint64_t> sub_sizes;      // every substream, folder after folder
            std::vector<uint8_t> sub_has_crc;
            std::vector<uint32_t> sub_crc;
        };

        struct File {
            std::u16string name;
            bool empty_stream = false;
            bool empty_file = false;
            bool anti = false;
            bool has_attributes = false;
            uint32_t attributes = 0;
            bool has_time[3] = {false, false, false};   // created, accessed, modified
            uint64_t time[3] = {0, 0, 0};                // FILETIME
        };

        // A reader of a header's bytes that stops at the first fault
        class HeaderReader {
        public:
            HeaderReader(const uint8_t* p, size_t n, uint64_t origin, const limits& l) noexcept
            : _p(p), _n(n), _origin(origin), _limits(l) {
            }

            const optional<error>& failure() const noexcept {
                return _error;
            }

            size_t at() const noexcept {
                return _at;
            }

            size_t left() const noexcept {
                return _n - _at;
            }

            bool fail(errc code, const char* text) {
                if (!_error) {
                    _error = error(code, _origin + _at, string(text));
                }
                return false;
            }

            bool byte(uint8_t& v) {
                if (_at >= _n) {
                    return fail(errc::corrupt, "7z: the header ends early");
                }
                v = _p[_at++];
                return true;
            }

            // A number: the leading 1 bits of its first byte count the
            // bytes after it (little-endian), the rest of it is the top
            bool number(uint64_t& v) {
                uint8_t first;
                if (!byte(first)) {
                    return false;
                }
                v = 0;
                uint8_t mask = 0x80;
                for (int i = 0; i < 8; ++i, mask >>= 1) {
                    if (!(first & mask)) {
                        v |= uint64_t(first & (mask - 1)) << (8 * i);
                        return true;
                    }
                    uint8_t b;
                    if (!byte(b)) {
                        return false;
                    }
                    v |= uint64_t(b) << (8 * i);
                }
                return true;
            }

            // A count of things of at least `each` bytes: no more than the header can hold
            bool count(uint64_t& v, size_t each, uint64_t max) {
                if (!number(v)) {
                    return false;
                }
                if (v > max || (each && v > left() / each)) {
                    return fail(errc::corrupt, "7z: a count past what the header holds");
                }
                return true;
            }

            bool u32(uint32_t& v) {
                if (left() < 4) {
                    return fail(errc::corrupt, "7z: the header ends early");
                }
                v = le32(_p + _at);
                _at += 4;
                return true;
            }

            bool u64(uint64_t& v) {
                uint32_t a, b;
                if (!u32(a) || !u32(b)) {
                    return false;
                }
                v = uint64_t(a) | uint64_t(b) << 32;
                return true;
            }

            bool skip(uint64_t n) {
                if (n > left()) {
                    return fail(errc::corrupt, "7z: the header ends early");
                }
                _at += size_t(n);
                return true;
            }

            bool expect(uint8_t id) {
                uint8_t b;
                if (!byte(b)) {
                    return false;
                }
                return b == id || fail(errc::corrupt, "7z: an unexpected property in the header");
            }

            // n bits, the first in the top bit of the first byte
            bool bits(size_t n, std::vector<uint8_t>& out) {
                if ((n + 7) / 8 > left()) {
                    return fail(errc::corrupt, "7z: the header ends early");
                }
                out.assign(n, 0);
                for (size_t i = 0; i < n; ++i) {
                    out[i] = (_p[_at + i / 8] >> (7 - i % 8)) & 1;
                }
                _at += (n + 7) / 8;
                return true;
            }

            // A byte saying all are there, or a vector of which are
            bool defined(size_t n, std::vector<uint8_t>& out) {
                uint8_t all;
                if (!byte(all)) {
                    return false;
                }
                if (all) {
                    out.assign(n, 1);
                    return true;
                }
                return bits(n, out);
            }

            bool digests(size_t n, std::vector<uint8_t>& has, std::vector<uint32_t>& crc) {
                if (!defined(n, has)) {
                    return false;
                }
                crc.assign(n, 0);
                for (size_t i = 0; i < n; ++i) {
                    if (has[i] && !u32(crc[i])) {
                        return false;
                    }
                }
                return true;
            }

            bool streams(Streams& s) {
                uint8_t id;
                if (!byte(id)) {
                    return false;
                }
                if (id == kPackInfo) {
                    if (!_pack_info(s) || !byte(id)) {
                        return false;
                    }
                }
                if (id == kUnpackInfo) {
                    if (!_unpack_info(s) || !byte(id)) {
                        return false;
                    }
                }
                _default_substreams(s);
                if (id == kSubStreamsInfo) {
                    if (!_substreams(s) || !byte(id)) {
                        return false;
                    }
                }
                if (id != kEnd) {
                    return fail(errc::corrupt, "7z: an unexpected property in the streams");
                }
                return _assign_packs(s);
            }

            bool files(const Streams& s, std::vector<File>& files) {
                uint64_t n;
                if (!number(n)) {
                    return false;
                }
                if (n > _limits.max_entries) {
                    return fail(errc::too_large, "7z: more entries than the limit allows");
                }
                if (n > left()) {
                    return fail(errc::corrupt, "7z: a count past what the header holds");
                }
                files.assign(size_t(n), File());
                size_t empty = 0;
                for (;;) {
                    uint64_t type, size;
                    if (!number(type)) {
                        return false;
                    }
                    if (type == kEnd) {
                        break;
                    }
                    if (!number(size) || size > left()) {
                        return fail(errc::corrupt, "7z: a property past the header's end");
                    }
                    size_t end = _at + size_t(size);
                    std::vector<uint8_t> v;
                    switch (type) {
                        case kEmptyStream:
                            if (!bits(files.size(), v)) {
                                return false;
                            }
                            empty = 0;
                            for (size_t i = 0; i < files.size(); ++i) {
                                files[i].empty_stream = v[i];
                                empty += v[i];
                            }
                            break;
                        case kEmptyFile:
                        case kAnti: {
                            if (!bits(empty, v)) {
                                return false;
                            }
                            for (size_t i = 0, k = 0; i < files.size(); ++i) {
                                if (files[i].empty_stream) {
                                    (type == kEmptyFile ? files[i].empty_file : files[i].anti) = v[k++];
                                }
                            }
                            break;
                        }
                        case kName:
                            if (!_names(files, end)) {
                                return false;
                            }
                            break;
                        case kCTime:
                        case kATime:
                        case kMTime:
                        case kWinAttributes: {
                            uint8_t external;
                            if (!defined(files.size(), v) || !byte(external)) {
                                return false;
                            }
                            if (external) {
                                return fail(errc::unsupported, "7z: file properties in another stream");
                            }
                            for (size_t i = 0; i < files.size(); ++i) {
                                if (!v[i]) {
                                    continue;
                                }
                                File& f = files[i];
                                if (type == kWinAttributes) {
                                    if (!u32(f.attributes)) {
                                        return false;
                                    }
                                    f.has_attributes = true;
                                } else {
                                    int k = type == kCTime ? 0 : type == kATime ? 1 : 2;
                                    if (!u64(f.time[k])) {
                                        return false;
                                    }
                                    f.has_time[k] = true;
                                }
                            }
                            break;
                        }
                        default:
                            break;   // kDummy, kStartPos, kComment and what comes later: skipped
                    }
                    if (_at > end) {
                        return fail(errc::corrupt, "7z: a property longer than its size");
                    }
                    _at = end;
                }
                // the files with a stream take the substreams in order
                size_t with = 0;
                for (auto& f : files) {
                    with += !f.empty_stream;
                }
                if (with != s.sub_sizes.size()) {
                    return fail(errc::corrupt, "7z: the files do not match the streams");
                }
                return true;
            }

        private:
            bool _pack_info(Streams& s) {
                uint64_t n;
                if (!number(s.pack_pos) || !count(n, 1, MaxStreams * 4096)) {
                    return false;
                }
                s.pack_sizes.assign(size_t(n), 0);
                for (;;) {
                    uint8_t id;
                    if (!byte(id)) {
                        return false;
                    }
                    if (id == kEnd) {
                        return true;
                    }
                    if (id == kSize) {
                        for (auto& v : s.pack_sizes) {
                            if (!number(v)) {
                                return false;
                            }
                        }
                    } else if (id == kCRC) {
                        std::vector<uint8_t> has;
                        std::vector<uint32_t> crc;
                        if (!digests(s.pack_sizes.size(), has, crc)) {
                            return false;
                        }
                    } else {
                        return fail(errc::corrupt, "7z: an unexpected property in the pack info");
                    }
                }
            }

            bool _folder(Folder& f) {
                uint64_t n;
                if (!count(n, 2, MaxCoders)) {
                    return false;
                }
                if (n == 0) {
                    return fail(errc::corrupt, "7z: a folder of no coder");
                }
                f.coders.resize(size_t(n));
                uint32_t inputs = 0;
                for (auto& c : f.coders) {
                    uint8_t flags;
                    if (!byte(flags)) {
                        return false;
                    }
                    size_t id_size = flags & 0x0F;
                    if (flags & 0x80) {
                        return fail(errc::unsupported, "7z: alternative methods");
                    }
                    if (id_size > 8 || id_size > left()) {
                        return fail(errc::corrupt, "7z: a method ID past 8 bytes");
                    }
                    c.method = 0;
                    for (size_t i = 0; i < id_size; ++i) {
                        c.method = (c.method << 8) | _p[_at++];
                    }
                    if (flags & 0x10) {
                        uint64_t ins, outs;
                        if (!number(ins) || !number(outs)) {
                            return false;
                        }
                        if (ins == 0 || ins > MaxStreams || outs != 1) {
                            return fail(errc::unsupported, "7z: a coder of that many streams");
                        }
                        c.inputs = uint32_t(ins);
                    }
                    c.first_input = inputs;
                    inputs += c.inputs;
                    if (inputs > MaxStreams) {
                        return fail(errc::unsupported, "7z: a folder of that many streams");
                    }
                    if (flags & 0x20) {
                        uint64_t size;
                        if (!number(size) || size > left() || size > 0xFFFF) {
                            return fail(errc::corrupt, "7z: coder properties past the header's end");
                        }
                        c.props.assign(_p + _at, _p + _at + size);
                        _at += size_t(size);
                    }
                }
                f.inputs = inputs;
                size_t binds = f.coders.size() - 1;
                std::vector<uint8_t> in_bound(inputs, 0), out_bound(f.coders.size(), 0);
                for (size_t i = 0; i < binds; ++i) {
                    uint64_t in, out;
                    if (!number(in) || !number(out)) {
                        return false;
                    }
                    if (in >= inputs || out >= f.coders.size() || in_bound[size_t(in)] || out_bound[size_t(out)]) {
                        return fail(errc::corrupt, "7z: an invalid bind pair");
                    }
                    in_bound[size_t(in)] = out_bound[size_t(out)] = 1;
                    f.binds.push_back({uint32_t(in), uint32_t(out)});
                }
                size_t packed = inputs - binds;
                if (inputs < binds + 1) {
                    return fail(errc::corrupt, "7z: a folder with no packed stream");
                }
                if (packed == 1) {
                    for (uint32_t i = 0; i < inputs; ++i) {
                        if (!in_bound[i]) {
                            f.packed.push_back(i);
                            break;
                        }
                    }
                } else {
                    for (size_t i = 0; i < packed; ++i) {
                        uint64_t in;
                        if (!number(in)) {
                            return false;
                        }
                        if (in >= inputs || in_bound[size_t(in)]) {
                            return fail(errc::corrupt, "7z: an invalid packed stream index");
                        }
                        in_bound[size_t(in)] = 1;
                        f.packed.push_back(uint32_t(in));
                    }
                }
                if (f.packed.size() != packed) {
                    return fail(errc::corrupt, "7z: an invalid packed stream index");
                }
                for (uint32_t i = 0; i < f.coders.size(); ++i) {
                    if (!out_bound[i]) {
                        f.main = i;
                    }
                }
                return true;
            }

            bool _unpack_info(Streams& s) {
                uint64_t n;
                uint8_t external;
                if (!expect(kFolder) || !count(n, 3, UINT64_MAX) || !byte(external)) {
                    return false;
                }
                if (external) {
                    return fail(errc::unsupported, "7z: folders in another stream");
                }
                s.folders.resize(size_t(n));
                for (auto& f : s.folders) {
                    if (!_folder(f)) {
                        return false;
                    }
                }
                if (!expect(kCodersUnpackSize)) {
                    return false;
                }
                for (auto& f : s.folders) {
                    f.sizes.resize(f.coders.size());
                    for (auto& v : f.sizes) {
                        if (!number(v)) {
                            return false;
                        }
                    }
                }
                for (;;) {
                    uint8_t id;
                    if (!byte(id)) {
                        return false;
                    }
                    if (id == kEnd) {
                        return true;
                    }
                    if (id != kCRC) {
                        return fail(errc::corrupt, "7z: an unexpected property in the unpack info");
                    }
                    std::vector<uint8_t> has;
                    std::vector<uint32_t> crc;
                    if (!digests(s.folders.size(), has, crc)) {
                        return false;
                    }
                    for (size_t i = 0; i < s.folders.size(); ++i) {
                        s.folders[i].has_crc = has[i];
                        s.folders[i].crc = crc[i];
                    }
                }
            }

            // One substream per folder, the folder's size and CRC
            void _default_substreams(Streams& s) {
                s.sub_sizes.clear();
                s.sub_has_crc.clear();
                s.sub_crc.clear();
                for (auto& f : s.folders) {
                    f.substreams = 1;
                    s.sub_sizes.push_back(f.size());
                    s.sub_has_crc.push_back(f.has_crc);
                    s.sub_crc.push_back(f.crc);
                }
            }

            bool _substreams(Streams& s) {
                uint8_t id;
                if (!byte(id)) {
                    return false;
                }
                uint64_t total = s.folders.size();
                if (id == kNumUnpackStream) {
                    total = 0;
                    for (auto& f : s.folders) {
                        if (!number(f.substreams)) {
                            return false;
                        }
                        total += f.substreams;
                        if (f.substreams > _limits.max_entries || total > _limits.max_entries) {
                            return fail(errc::too_large, "7z: more entries than the limit allows");
                        }
                    }
                    if (!byte(id)) {
                        return false;
                    }
                }
                s.sub_sizes.assign(size_t(total), 0);
                s.sub_has_crc.assign(size_t(total), 0);
                s.sub_crc.assign(size_t(total), 0);
                // the sizes: every one but the last of a folder given, the last the rest
                size_t k = 0;
                for (auto& f : s.folders) {
                    if (f.substreams == 0) {
                        continue;
                    }
                    uint64_t sum = 0;
                    for (uint64_t i = 0; i + 1 < f.substreams; ++i) {
                        uint64_t v = 0;
                        if (id == kSize && !number(v)) {
                            return false;
                        }
                        if (id != kSize) {
                            return fail(errc::corrupt, "7z: substreams without their sizes");
                        }
                        if (v > f.size() - sum) {
                            return fail(errc::corrupt, "7z: substreams larger than their folder");
                        }
                        sum += v;
                        s.sub_sizes[k++] = v;
                    }
                    s.sub_sizes[k++] = f.size() - sum;
                }
                if (id == kSize && !byte(id)) {
                    return false;
                }
                // the CRCs of the substreams whose folder's CRC does not stand for them
                size_t unknown = 0;
                for (auto& f : s.folders) {
                    if (f.substreams != 1 || !f.has_crc) {
                        unknown += size_t(f.substreams);
                    }
                }
                std::vector<uint8_t> has(unknown, 0);
                std::vector<uint32_t> crc(unknown, 0);
                for (;;) {
                    if (id == kEnd) {
                        break;
                    }
                    if (id == kCRC) {
                        if (!digests(unknown, has, crc)) {
                            return false;
                        }
                    } else {
                        return fail(errc::corrupt, "7z: an unexpected property in the substreams");
                    }
                    if (!byte(id)) {
                        return false;
                    }
                }
                k = 0;
                size_t u = 0;
                for (auto& f : s.folders) {
                    if (f.substreams == 1 && f.has_crc) {
                        s.sub_has_crc[k] = 1;
                        s.sub_crc[k] = f.crc;
                        ++k;
                        continue;
                    }
                    for (uint64_t i = 0; i < f.substreams; ++i, ++k, ++u) {
                        s.sub_has_crc[k] = has[u];
                        s.sub_crc[k] = crc[u];
                    }
                }
                return true;
            }

            // Every folder's first packed stream, and where each stream lies
            bool _assign_packs(Streams& s) {
                size_t next = 0;
                for (auto& f : s.folders) {
                    f.first_pack = next;
                    next += f.packed.size();
                }
                if (next > s.pack_sizes.size()) {
                    return fail(errc::corrupt, "7z: folders need more packed streams than there are");
                }
                uint64_t at = SignatureSize + s.pack_pos;
                if (at < s.pack_pos) {
                    return fail(errc::corrupt, "7z: packed streams past the archive's end");
                }
                s.pack_offsets.resize(s.pack_sizes.size());
                for (size_t i = 0; i < s.pack_sizes.size(); ++i) {
                    s.pack_offsets[i] = at;
                    if (at + s.pack_sizes[i] < at) {
                        return fail(errc::corrupt, "7z: packed streams past the archive's end");
                    }
                    at += s.pack_sizes[i];
                }
                return true;
            }

            bool _names(std::vector<File>& files, size_t end) {
                uint8_t external;
                if (!byte(external)) {
                    return false;
                }
                if (external) {
                    return fail(errc::unsupported, "7z: names in another stream");
                }
                for (auto& f : files) {
                    for (;;) {
                        if (end - _at < 2 || _at + 2 > _n) {
                            return fail(errc::corrupt, "7z: a name past the property's end");
                        }
                        char16_t c = char16_t(_p[_at] | _p[_at + 1] << 8);
                        _at += 2;
                        if (c == 0) {
                            break;
                        }
                        f.name.push_back(c);
                    }
                }
                return true;
            }

            const uint8_t* _p;
            size_t _n;
            size_t _at = 0;
            uint64_t _origin;
            limits _limits;
            optional<error> _error;
        };

        // UTF-16LE to UTF-8; nullopt for a surrogate without its pair
        inline optional<std::string> utf16_to_utf8(const std::u16string& s) {
            std::string out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size(); ++i) {
                uint32_t c = s[i];
                if (c >= 0xD800 && c < 0xDC00) {
                    if (i + 1 == s.size() || s[i + 1] < 0xDC00 || s[i + 1] >= 0xE000) {
                        return nullopt;
                    }
                    c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
                } else if (c >= 0xDC00 && c < 0xE000) {
                    return nullopt;
                }
                if (c < 0x80) {
                    out += char(c);
                } else if (c < 0x800) {
                    out += char(0xC0 | (c >> 6));
                    out += char(0x80 | (c & 0x3F));
                } else if (c < 0x10000) {
                    out += char(0xE0 | (c >> 12));
                    out += char(0x80 | ((c >> 6) & 0x3F));
                    out += char(0x80 | (c & 0x3F));
                } else {
                    out += char(0xF0 | (c >> 18));
                    out += char(0x80 | ((c >> 12) & 0x3F));
                    out += char(0x80 | ((c >> 6) & 0x3F));
                    out += char(0x80 | (c & 0x3F));
                }
            }
            return out;
        }
    }
}
