//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bcj.h"
#include "deflate.h"
#include "lzma2.h"
#include "ppmd7.h"
#include "sevenzip_aes.h"
#include "sevenzip_header.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::compress::detail {
    // The writing half of 7z: the filter an entry's first bytes call for,
    // a folder's coders encoding data as it comes, and the header written
    // at the end. Plain memory throughout (the writer's managed state owns
    // it): the records hold std strings, never library strings.
    namespace sevenzip_writing {
        using namespace sevenzip_format;

        enum class Method : uint8_t {
            lzma2,
            lzma,
            ppmd,
            deflate,
            copy
        };

        // The filter before the folder's coder: none, a branch converter, Delta
        struct FilterChoice {
            bool on = false;
            SimpleKind kind = SimpleKind::x86;
            uint32_t distance = 0;   // Delta's

            friend bool operator==(const FilterChoice& a, const FilterChoice& b) noexcept {
                return a.on == b.on && (!a.on || (a.kind == b.kind && a.distance == b.distance));
            }
        };

        inline uint32_t le16(const uint8_t* p) noexcept {
            return uint32_t(p[0]) | uint32_t(p[1]) << 8;
        }

        // What an entry's first bytes are, as 7-Zip's automatic filters
        // judge it: a program of ELF, Mach-O or PE for a processor whose
        // branches a converter knows (by the machine field of its header),
        // or a WAV file of PCM samples (Delta by the block align). The name
        // does not decide: a file named .exe that is not a PE gets nothing.
        inline FilterChoice detect(const uint8_t* h, size_t n) noexcept {
            FilterChoice none;
            auto pick = [](SimpleKind k) {
                FilterChoice f;
                f.on = true;
                f.kind = k;
                return f;
            };
            if (n >= 20 && h[0] == 0x7F && h[1] == 'E' && h[2] == 'L' && h[3] == 'F') {
                bool big = h[5] == 2;
                uint32_t machine = big ? (uint32_t(h[18]) << 8 | h[19]) : le16(h + 18);
                switch (machine) {
                    case 0x03: case 0x3E: return pick(SimpleKind::x86);
                    case 0x28: return pick(SimpleKind::arm);
                    case 0xB7: return pick(SimpleKind::arm64);
                    case 0xF3: return pick(SimpleKind::riscv);
                    case 0x14: case 0x15: return big ? pick(SimpleKind::powerpc) : none;
                    case 0x02: case 0x12: case 0x2B: return big ? pick(SimpleKind::sparc) : none;
                    case 0x32: return pick(SimpleKind::ia64);
                    default: return none;
                }
            }
            if (n >= 8 && (h[0] == 0xCE || h[0] == 0xCF) && h[1] == 0xFA && h[2] == 0xED && h[3] == 0xFE) {
                uint32_t cpu = le32(h + 4);
                switch (cpu) {
                    case 7: case 0x01000007: return pick(SimpleKind::x86);
                    case 12: return pick(SimpleKind::arm);
                    case 0x0100000C: return pick(SimpleKind::arm64);
                    default: return none;
                }
            }
            if (n >= 8 && h[0] == 0xFE && h[1] == 0xED && h[2] == 0xFA && (h[3] == 0xCE || h[3] == 0xCF)) {
                uint32_t cpu = uint32_t(h[4]) << 24 | uint32_t(h[5]) << 16 | uint32_t(h[6]) << 8 | h[7];
                return cpu == 18 || cpu == 0x01000012 ? pick(SimpleKind::powerpc) : none;
            }
            if (n >= 0x40 && h[0] == 'M' && h[1] == 'Z') {
                uint32_t off = le32(h + 0x3C);
                if (off <= n - 6 && std::memcmp(h + off, "PE\0\0", 4) == 0) {
                    switch (le16(h + off + 4)) {
                        case 0x14C: case 0x8664: return pick(SimpleKind::x86);
                        case 0xAA64: return pick(SimpleKind::arm64);
                        case 0x1C0: return pick(SimpleKind::arm);
                        case 0x1C2: case 0x1C4: return pick(SimpleKind::armt);
                        case 0x200: return pick(SimpleKind::ia64);
                        case 0x5032: case 0x5064: return pick(SimpleKind::riscv);
                        default: return none;
                    }
                }
                return none;
            }
            if (n >= 36 && std::memcmp(h, "RIFF", 4) == 0 && std::memcmp(h + 8, "WAVEfmt ", 8) == 0) {
                uint32_t format = le16(h + 20);
                uint32_t align = le16(h + 32);
                if ((format == 1 || format == 0xFFFE) && align >= 1 && align <= 256) {
                    FilterChoice f;
                    f.on = true;
                    f.kind = SimpleKind::delta;
                    f.distance = align;
                    return f;
                }
            }
            return none;
        }

        // The ID a filter has in a folder
        inline uint64_t filter_method(const FilterChoice& f) noexcept {
            switch (f.kind) {
                case SimpleKind::x86: return X86;
                case SimpleKind::powerpc: return PowerPC;
                case SimpleKind::ia64: return Ia64;
                case SimpleKind::arm: return Arm;
                case SimpleKind::armt: return ArmThumb;
                case SimpleKind::sparc: return Sparc;
                case SimpleKind::arm64: return Arm64;
                case SimpleKind::riscv: return Riscv;
                case SimpleKind::delta: return Delta;
            }
            return 0;
        }

        struct CoderRecord {
            uint64_t method = 0;
            std::vector<uint8_t> props;
        };

        struct FolderRecord {
            std::vector<CoderRecord> coders;   // the coder of the packed stream first, then the filter (the folder's output), as 7-Zip lists them
            std::vector<uint64_t> coder_sizes; // each coder's output, in that order
            uint64_t unpacked = 0;
            uint64_t packed = 0;
            std::vector<uint64_t> sizes;       // its entries' data
            std::vector<uint32_t> crcs;
        };

        struct FileRecord {
            std::u16string name;
            bool stream = false;
            bool directory = false;
            uint64_t size = 0;
            uint32_t crc = 0;
            uint32_t attributes = 0;
            bool has_time[3] = {false, false, false};   // created, accessed, modified
            uint64_t time[3] = {0, 0, 0};                // FILETIME
        };

        inline uint64_t filetime_of_unix_nano(int64_t ns) noexcept {
            return uint64_t(ns / 100 + 116444736000000000LL);
        }

        inline uint64_t filetime_now() noexcept {
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            return filetime_of_unix_nano(ns);
        }

        // UTF-8 to UTF-16LE's code units; false for bytes that are not UTF-8
        inline bool utf8_to_utf16(std::string_view s, std::u16string& out) {
            out.clear();
            for (size_t i = 0; i < s.size();) {
                uint8_t c = uint8_t(s[i]);
                uint32_t cp;
                size_t len;
                if (c < 0x80) {
                    cp = c;
                    len = 1;
                } else if ((c & 0xE0) == 0xC0 && c >= 0xC2) {
                    cp = c & 0x1F;
                    len = 2;
                } else if ((c & 0xF0) == 0xE0) {
                    cp = c & 0x0F;
                    len = 3;
                } else if ((c & 0xF8) == 0xF0 && c <= 0xF4) {
                    cp = c & 0x07;
                    len = 4;
                } else {
                    return false;
                }
                if (i + len > s.size()) {
                    return false;
                }
                for (size_t k = 1; k < len; ++k) {
                    uint8_t d = uint8_t(s[i + k]);
                    if ((d & 0xC0) != 0x80) {
                        return false;
                    }
                    cp = (cp << 6) | (d & 0x3F);
                }
                if ((len == 3 && (cp < 0x800 || (cp >= 0xD800 && cp < 0xE000))) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF))) {
                    return false;
                }
                if (cp >= 0x10000) {
                    cp -= 0x10000;
                    out.push_back(char16_t(0xD800 + (cp >> 10)));
                    out.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
                } else {
                    out.push_back(char16_t(cp));
                }
                i += len;
            }
            return true;
        }

        // A dictionary no larger than the data needs: the next 2^k or 3 * 2^(k-1)
        inline uint32_t round_dictionary(uint64_t n, uint32_t cap) noexcept {
            for (uint32_t b = 12; b < 32; ++b) {
                uint64_t two = uint64_t(1) << b;
                if (two >= n) {
                    return uint32_t(std::min<uint64_t>(two, cap));
                }
                if (two / 2 * 3 >= n) {
                    return uint32_t(std::min<uint64_t>(two / 2 * 3, cap));
                }
            }
            return cap;
        }

        // PPMd at a level, as 7-Zip sets it: the order from a table, the
        // memory 2^(level + 19) (192 MiB at 9)
        inline void ppmd_of_level(int level, uint32_t& order, uint32_t& memory) noexcept {
            static constexpr uint8_t orders[10] = {3, 4, 4, 5, 5, 6, 8, 16, 24, 32};
            order = orders[level];
            memory = level >= 9 ? uint32_t(192) << 20 : uint32_t(1) << (level + 19);
        }

        // A folder's coders encoding its data as it comes: one encoder a
        // writer, its memory kept from folder to folder
        class FolderEncoder {
        public:
            FolderEncoder(Method m, int level)
            : _method(m), _level(level) {
                if (m == Method::lzma || m == Method::lzma2) {
                    _settings = LzmaEncoderSettings::of(level, false);
                }
                if (m == Method::ppmd) {
                    ppmd_of_level(level, _order, _memory);
                }
            }

            // With a password: every folder encrypted, 7zAES the coder of
            // its packed stream; one salt a writer (one key), a fresh IV a
            // folder
            void encrypt(crypto::secret<32> key, const uint8_t* salt) {
                _key = std::make_unique<crypto::secret<32>>(std::move(key));
                std::memcpy(_salt, salt, 16);
            }

            const crypto::secret<32>* key() const noexcept {
                return _key.get();
            }

            const uint8_t* salt() const noexcept {
                return _salt;
            }

            void start(const FilterChoice& f) {
                _filter = f;
                _chain.clear();
                if (_key) {
                    sevenzip_aes::random_bytes(_iv, 16);
                    _cbc = std::make_unique<sevenzip_aes::Cbc>(*_key, _iv);
                    _held = 0;
                    _coded = 0;
                }
                if (f.on) {
                    SimpleFilter s;
                    s.init(f.kind, true, 0, f.kind == SimpleKind::delta ? f.distance : 1);
                    _chain.add(s);
                }
                switch (_method) {
                    case Method::lzma2:
                        if (!_lzma2) {
                            _lzma2 = std::make_unique<Lzma2Encoder>(_settings);
                        } else {
                            _lzma2->restart();
                        }
                        break;
                    case Method::lzma:
                        if (!_lzma) {
                            _lzma = std::make_unique<LzmaEncoder>(_settings);
                        } else {
                            _lzma->restart();
                        }
                        break;
                    case Method::ppmd:
                        if (!_ppmd) {
                            _ppmd = std::make_unique<Ppmd7Encoder>(_order, _memory);
                        } else {
                            _ppmd->restart();
                        }
                        break;
                    case Method::deflate:
                        if (!_deflate) {
                            _deflate = std::make_unique<Deflater>(_level);
                        } else {
                            _deflate->reset();
                        }
                        break;
                    case Method::copy:
                        break;
                }
                _unpacked = 0;
            }

            uint64_t unpacked() const noexcept {
                return _unpacked;
            }

            void write(const uint8_t* p, size_t n, std::vector<uint8_t>& out) {
                _unpacked += n;
                size_t base = _begin(out);
                if (_chain.empty()) {
                    _code(p, n, out);
                } else {
                    while (n) {
                        size_t k = std::min(n, _chain.room());
                        _chain.push(p, k);
                        p += k;
                        n -= k;
                        _chain.run(false);
                        size_t r = _chain.ready_size();
                        _code(_chain.ready(), r, out);
                        _chain.take(r);
                    }
                }
                _seal(out, base, false);
            }

            void finish(std::vector<uint8_t>& out) {
                size_t base = _begin(out);
                if (!_chain.empty()) {
                    _chain.run(true);
                    size_t r = _chain.ready_size();
                    _code(_chain.ready(), r, out);
                    _chain.take(r);
                }
                switch (_method) {
                    case Method::lzma2:
                        (void)_lzma2->run(true, out);
                        _lzma2->finish(out);
                        break;
                    case Method::lzma:
                        (void)_lzma->run(true, out);
                        _lzma->finish(false, out);
                        break;
                    case Method::ppmd:
                        _ppmd->finish(out);
                        break;
                    case Method::deflate:
                        _deflate->finish(out);
                        break;
                    case Method::copy:
                        break;
                }
                _seal(out, base, true);
                _cbc.reset();
            }

            // The folder's coders as 7-Zip lists them: the coder of the
            // packed stream (its dictionary no larger than the folder), then
            // the filter, fed by it
            std::vector<CoderRecord> coders() const {
                std::vector<CoderRecord> c;
                CoderRecord m;
                switch (_method) {
                    case Method::lzma2: {
                        m.method = Lzma2;
                        uint32_t d = round_dictionary(_unpacked, std::max(_settings.props.dictionary, lzma_model::DictionaryMin));
                        m.props.push_back(lzma2_dictionary_byte(d));
                        break;
                    }
                    case Method::lzma: {
                        m.method = Lzma;
                        uint32_t d = round_dictionary(_unpacked, std::max(_settings.props.dictionary, lzma_model::DictionaryMin));
                        m.props.push_back(_settings.props.to_byte());
                        for (int i = 0; i < 4; ++i) {
                            m.props.push_back(uint8_t(d >> (8 * i)));
                        }
                        break;
                    }
                    case Method::ppmd:
                        m.method = Ppmd;
                        m.props.push_back(uint8_t(_order));
                        for (int i = 0; i < 4; ++i) {
                            m.props.push_back(uint8_t(_memory >> (8 * i)));
                        }
                        break;
                    case Method::deflate:
                        m.method = Deflate;
                        break;
                    case Method::copy:
                        m.method = Copy;
                        break;
                }
                if (_key) {
                    CoderRecord a;
                    a.method = Aes;
                    a.props = sevenzip_aes::props(sevenzip_aes::WriteRounds, _salt, _iv);
                    c.push_back(std::move(a));
                }
                c.push_back(m);
                if (_filter.on) {
                    CoderRecord f;
                    f.method = filter_method(_filter);
                    if (_filter.kind == SimpleKind::delta) {
                        f.props.push_back(uint8_t(_filter.distance - 1));
                    }
                    c.push_back(f);
                }
                return c;
            }

            // Each coder's output, as coders() lists them: the encrypted
            // coder's (unpadded), the data's
            std::vector<uint64_t> coder_sizes() const {
                std::vector<uint64_t> s;
                if (_key) {
                    s.push_back(_coded);
                }
                s.push_back(_unpacked);
                if (_filter.on) {
                    s.push_back(_unpacked);
                }
                return s;
            }

            // The solid block 7-Zip uses for these settings: 128 times the
            // dictionary (LZMA, LZMA2) or 16 times the model's memory
            // (PPMd), within 16 MiB .. 4 GiB; 16 MiB for Deflate and Copy
            uint64_t solid_block() const noexcept {
                uint64_t lo = uint64_t(16) << 20, hi = uint64_t(4) << 30;
                uint64_t v = lo;
                if (_method == Method::lzma || _method == Method::lzma2) {
                    v = uint64_t(_settings.props.dictionary) * 128;
                } else if (_method == Method::ppmd) {
                    v = uint64_t(_memory) * 16;
                }
                return std::clamp(v, lo, hi);
            }

        private:
            // Where the coder's bytes of this call start in out; the bytes
            // held from the last call (short of a block) put back before them
            size_t _begin(std::vector<uint8_t>& out) {
                size_t base = out.size();
                _carried = _held;
                if (_cbc && _held) {
                    out.insert(out.end(), _hold, _hold + _held);
                    _held = 0;
                }
                return base;
            }

            // The whole blocks from base encrypted in place, the rest held
            // (the last call pads it with zeros to a block)
            void _seal(std::vector<uint8_t>& out, size_t base, bool last) {
                if (!_cbc) {
                    return;
                }
                size_t n = out.size() - base;
                _coded += n - _carried;
                if (last && (n & 15)) {
                    out.resize(out.size() + (16 - (n & 15)), 0);
                    n = out.size() - base;
                }
                size_t whole = n & ~size_t(15);
                _cbc->encrypt(out.data() + base, whole);
                _held = n - whole;
                std::memcpy(_hold, out.data() + base + whole, _held);
                out.resize(base + whole);
            }

            void _code(const uint8_t* p, size_t n, std::vector<uint8_t>& out) {
                switch (_method) {
                    case Method::lzma2:
                        while (n) {
                            size_t k = _lzma2->append(p, n);
                            p += k;
                            n -= k;
                            (void)_lzma2->run(false, out);
                        }
                        break;
                    case Method::lzma:
                        while (n) {
                            size_t k = _lzma->append(p, n);
                            p += k;
                            n -= k;
                            (void)_lzma->run(false, out);
                        }
                        break;
                    case Method::ppmd:
                        _ppmd->encode(p, n, out);
                        break;
                    case Method::deflate:
                        _deflate->write(p, n, out);
                        break;
                    case Method::copy:
                        out.insert(out.end(), p, p + n);
                        break;
                }
            }

            Method _method;
            int _level;
            LzmaEncoderSettings _settings;
            uint32_t _order = 6;
            uint32_t _memory = uint32_t(16) << 20;
            FilterChoice _filter;
            FilterChain _chain;
            std::unique_ptr<Lzma2Encoder> _lzma2;
            std::unique_ptr<LzmaEncoder> _lzma;
            std::unique_ptr<Ppmd7Encoder> _ppmd;
            std::unique_ptr<Deflater> _deflate;
            uint64_t _unpacked = 0;
            std::unique_ptr<crypto::secret<32>> _key;   // the password's key (zeroed when dropped)
            uint8_t _salt[16] = {};
            uint8_t _iv[16] = {};
            std::unique_ptr<sevenzip_aes::Cbc> _cbc;
            uint8_t _hold[16] = {};
            size_t _held = 0;
            size_t _carried = 0;
            uint64_t _coded = 0;                        // the coder's output, before the padding
        };

        // A number of the header: the leading 1 bits of the first byte
        // count the bytes after it, the rest of it the top
        inline void put_number(std::vector<uint8_t>& out, uint64_t v) {
            uint8_t first = 0;
            uint8_t mask = 0x80;
            int i = 0;
            for (; i < 8; ++i) {
                if (v < (uint64_t(1) << (7 * (i + 1)))) {
                    first |= uint8_t(v >> (8 * i));
                    break;
                }
                first |= mask;
                mask >>= 1;
            }
            out.push_back(first);
            for (int k = 0; k < i; ++k) {
                out.push_back(uint8_t(v >> (8 * k)));
            }
        }

        inline void put_bits(std::vector<uint8_t>& out, const std::vector<uint8_t>& bits) {
            uint8_t b = 0;
            size_t k = 0;
            for (auto v : bits) {
                b |= uint8_t((v ? 1 : 0) << (7 - k));
                if (++k == 8) {
                    out.push_back(b);
                    b = 0;
                    k = 0;
                }
            }
            if (k) {
                out.push_back(b);
            }
        }

        inline void put_coder(std::vector<uint8_t>& out, const CoderRecord& c) {
            uint8_t id[8];
            size_t n = 0;
            uint64_t m = c.method;
            do {
                id[n++] = uint8_t(m);
                m >>= 8;
            } while (m);
            out.push_back(uint8_t(n | (c.props.empty() ? 0 : 0x20)));
            for (size_t i = n; i-- > 0;) {
                out.push_back(id[i]);
            }
            if (!c.props.empty()) {
                put_number(out, c.props.size());
                out.insert(out.end(), c.props.begin(), c.props.end());
            }
        }

        // One folder's coders: a chain, each fed by the one before it, the
        // packed stream into the first (the input nothing binds)
        inline void put_folder(std::vector<uint8_t>& out, const std::vector<CoderRecord>& coders) {
            put_number(out, coders.size());
            for (auto& c : coders) {
                put_coder(out, c);
            }
            for (size_t i = 1; i < coders.size(); ++i) {
                put_number(out, i);       // the input of coder i (one input each)
                put_number(out, i - 1);   // fed by coder i-1's output
            }
        }

        // The plain header of these folders and files
        inline std::vector<uint8_t> header(const std::vector<FolderRecord>& folders, const std::vector<FileRecord>& files) {
            std::vector<uint8_t> h;
            h.push_back(kHeader);
            if (!folders.empty()) {
                h.push_back(kMainStreamsInfo);
                h.push_back(kPackInfo);
                put_number(h, 0);
                put_number(h, folders.size());
                h.push_back(kSize);
                for (auto& f : folders) {
                    put_number(h, f.packed);
                }
                h.push_back(kEnd);
                h.push_back(kUnpackInfo);
                h.push_back(kFolder);
                put_number(h, folders.size());
                h.push_back(0);
                for (auto& f : folders) {
                    put_folder(h, f.coders);
                }
                h.push_back(kCodersUnpackSize);
                for (auto& f : folders) {
                    if (f.coder_sizes.size() == f.coders.size()) {
                        for (auto v : f.coder_sizes) {
                            put_number(h, v);
                        }
                    } else {
                        for (size_t i = 0; i < f.coders.size(); ++i) {
                            put_number(h, f.unpacked);   // every coder keeps the length
                        }
                    }
                }
                h.push_back(kEnd);
                h.push_back(kSubStreamsInfo);
                bool counts = false;
                for (auto& f : folders) {
                    counts |= f.sizes.size() != 1;
                }
                if (counts) {
                    h.push_back(kNumUnpackStream);
                    for (auto& f : folders) {
                        put_number(h, f.sizes.size());
                    }
                }
                bool sizes = false;
                for (auto& f : folders) {
                    sizes |= f.sizes.size() > 1;
                }
                if (sizes) {
                    h.push_back(kSize);
                    for (auto& f : folders) {
                        for (size_t i = 0; i + 1 < f.sizes.size(); ++i) {
                            put_number(h, f.sizes[i]);
                        }
                    }
                }
                h.push_back(kCRC);
                h.push_back(1);   // all defined
                for (auto& f : folders) {
                    for (auto c : f.crcs) {
                        put_le32(h, c);
                    }
                }
                h.push_back(kEnd);
                h.push_back(kEnd);
            }
            if (!files.empty()) {
                h.push_back(kFilesInfo);
                put_number(h, files.size());
                std::vector<uint8_t> v;
                size_t empty = 0;
                for (auto& f : files) {
                    empty += !f.stream;
                }
                if (empty) {
                    std::vector<uint8_t> bits;
                    for (auto& f : files) {
                        bits.push_back(!f.stream);
                    }
                    v.clear();
                    put_bits(v, bits);
                    h.push_back(kEmptyStream);
                    put_number(h, v.size());
                    h.insert(h.end(), v.begin(), v.end());
                    bits.clear();
                    bool any_file = false;
                    for (auto& f : files) {
                        if (!f.stream) {
                            bits.push_back(!f.directory);
                            any_file |= !f.directory;
                        }
                    }
                    if (any_file) {
                        v.clear();
                        put_bits(v, bits);
                        h.push_back(kEmptyFile);
                        put_number(h, v.size());
                        h.insert(h.end(), v.begin(), v.end());
                    }
                }
                v.clear();
                v.push_back(0);   // not external
                for (auto& f : files) {
                    for (char16_t c : f.name) {
                        v.push_back(uint8_t(c));
                        v.push_back(uint8_t(c >> 8));
                    }
                    v.push_back(0);
                    v.push_back(0);
                }
                h.push_back(kName);
                put_number(h, v.size());
                h.insert(h.end(), v.begin(), v.end());
                static constexpr uint8_t ids[3] = {kCTime, kATime, kMTime};
                for (int k = 0; k < 3; ++k) {
                    std::vector<uint8_t> defined;
                    size_t count = 0;
                    for (auto& f : files) {
                        defined.push_back(f.has_time[k]);
                        count += f.has_time[k];
                    }
                    if (!count) {
                        continue;
                    }
                    v.clear();
                    if (count == files.size()) {
                        v.push_back(1);
                    } else {
                        v.push_back(0);
                        put_bits(v, defined);
                    }
                    v.push_back(0);   // not external
                    for (auto& f : files) {
                        if (f.has_time[k]) {
                            put_le32(v, uint32_t(f.time[k]));
                            put_le32(v, uint32_t(f.time[k] >> 32));
                        }
                    }
                    h.push_back(ids[k]);
                    put_number(h, v.size());
                    h.insert(h.end(), v.begin(), v.end());
                }
                v.clear();
                v.push_back(1);   // all defined
                v.push_back(0);   // not external
                for (auto& f : files) {
                    put_le32(v, f.attributes);
                }
                h.push_back(kWinAttributes);
                put_number(h, v.size());
                h.insert(h.end(), v.begin(), v.end());
                h.push_back(kEnd);
            }
            h.push_back(kEnd);
            return h;
        }

        // The header packed with LZMA (as 7-Zip packs it): the packed bytes,
        // and the small header that describes them (kEncodedHeader), whose
        // stream lies `pack_pos` bytes after the signature header; with a
        // key, the packed bytes encrypted too (7zAES before LZMA)
        inline void packed_header(const std::vector<uint8_t>& plain, uint64_t pack_pos, std::vector<uint8_t>& packed, std::vector<uint8_t>& encoded,
                                  const crypto::secret<32>* key = nullptr, const uint8_t* salt = nullptr) {
            auto s = LzmaEncoderSettings::of(5, false);
            s.props.dictionary = uint32_t(1) << 20;
            auto enc = std::make_unique<LzmaEncoder>(s);
            enc->attach(plain.data(), plain.size());
            (void)enc->run(true, packed);
            enc->finish(false, packed);
            CoderRecord c;
            c.method = Lzma;
            c.props.push_back(s.props.to_byte());
            for (int i = 0; i < 4; ++i) {
                c.props.push_back(uint8_t(enc->dictionary() >> (8 * i)));
            }
            std::vector<CoderRecord> coders;
            std::vector<uint64_t> sizes;
            if (key) {
                uint8_t iv[16];
                sevenzip_aes::random_bytes(iv, 16);
                CoderRecord a;
                a.method = Aes;
                a.props = sevenzip_aes::props(sevenzip_aes::WriteRounds, salt, iv);
                coders.push_back(std::move(a));
                sizes.push_back(packed.size());
                packed.resize((packed.size() + 15) & ~size_t(15), 0);
                sevenzip_aes::Cbc(*key, iv).encrypt(packed.data(), packed.size());
            }
            coders.push_back(std::move(c));
            sizes.push_back(plain.size());
            auto& e = encoded;
            e.push_back(kEncodedHeader);
            e.push_back(kPackInfo);
            put_number(e, pack_pos);
            put_number(e, 1);
            e.push_back(kSize);
            put_number(e, packed.size());
            e.push_back(kEnd);
            e.push_back(kUnpackInfo);
            e.push_back(kFolder);
            put_number(e, 1);
            e.push_back(0);
            put_folder(e, coders);
            e.push_back(kCodersUnpackSize);
            for (auto v : sizes) {
                put_number(e, v);
            }
            e.push_back(kCRC);
            e.push_back(1);
            put_le32(e, hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(plain.data()), plain.size())));
            e.push_back(kEnd);
            e.push_back(kEnd);
        }

        // The signature header of an archive whose header lies `offset`
        // bytes after it
        inline std::vector<uint8_t> signature(uint64_t offset, const std::vector<uint8_t>& header) {
            std::vector<uint8_t> s(Signature, Signature + 6);
            s.push_back(0);
            s.push_back(4);
            std::vector<uint8_t> start;
            put_le32(start, uint32_t(offset));
            put_le32(start, uint32_t(offset >> 32));
            put_le32(start, uint32_t(header.size()));
            put_le32(start, uint32_t(uint64_t(header.size()) >> 32));
            put_le32(start, header.empty() ? 0 : hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(header.data()), header.size())));
            put_le32(s, hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(start.data()), start.size())));
            s.insert(s.end(), start.begin(), start.end());
            return s;
        }
    }
}
