//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec_stream.h"
#include "lz4_block.h"
#include "../../hash/xxhash.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

// The LZ4 frame format (LZ4 Frame Format Description 1.6): a magic number,
// a descriptor (FLG: version 01, independent blocks, block checksums, the
// content size, a content checksum, a dictionary id; BD: the largest block,
// 64 KB to 4 MB; the fields FLG asks for; a byte of XXH32 over them), the
// blocks — four bytes of size, the high bit set for a block stored as it is
// — each with the XXH32 of its bytes when asked, an end mark of four zero
// bytes, and the XXH32 of the content when asked. Skippable frames
// (0x184D2A5x, a size, bytes) are passed over; the legacy frame of lz4 -l
// (0x184C2102, then blocks of 8 MB, each its compressed size alone, until
// the input ends or another frame begins) is read.
namespace sgcl::compress::detail {
    inline constexpr uint32_t Lz4Magic = 0x184D2204u;
    inline constexpr uint32_t Lz4LegacyMagic = 0x184C2102u;
    inline constexpr uint32_t Lz4SkippableMagic = 0x184D2A50u;   // to 0x184D2A5F
    inline constexpr size_t Lz4LegacyBlock = size_t(8) << 20;
    inline constexpr uint32_t Lz4Stored = 0x80000000u;

    SGCL_INLINE_HOT constexpr size_t lz4_block_bytes(unsigned id) noexcept {
        return size_t(1) << (8 + 2 * id);   // 4: 64 KB, 5: 256 KB, 6: 1 MB, 7: 4 MB
    }

    SGCL_INLINE_HOT uint32_t lz4_le32(const uint8_t* p) noexcept {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }

    SGCL_INLINE_HOT bool lz4_skippable(uint32_t magic) noexcept {
        return (magic & 0xFFFFFFF0u) == Lz4SkippableMagic;
    }

    template<class Out>
    SGCL_INLINE_HOT void lz4_put32(Out& out, uint32_t v) noexcept {
        const uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        append_bytes(out, b, 4);
    }

    // The compressor of a level, its tables kept from one block (and one
    // call) to the next: indexes only ever grow, so an entry of an earlier
    // block or call is below the start of what may be matched and is never
    // followed, and nothing is cleared between them. `next` is the first
    // index never used; before it would pass 2^31 the tables are shifted
    // down (or cleared, where nothing before is wanted).
    class Lz4Engine {
    public:
        static constexpr uint32_t Rebase = uint32_t(1) << 31;

        SGCL_INLINE_HOT int level() const noexcept {
            return _level;
        }

        // Ready for the level: the tables of its kind made on first need
        void set_level(int level) noexcept {
            _level = level;
            if (level == 2 && !_mid) {
                _mid = std::make_unique<Lz4Mid>();
            }
            if (level >= 3 && !_hc) {
                _hc = std::make_unique<Lz4Hc>();
            }
        }

        // The index a new stretch of n bytes may start at (with `keep`
        // bytes of history before it): the tables shifted when it would
        // pass 2^31
        uint32_t room(size_t n, size_t keep) noexcept {
            if (uint64_t(next) + n + Lz4Window >= Rebase) {
                // a multiple of the window: the chain's links sit at their
                // position modulo it, and stay where they are
                const uint32_t delta = (next > keep ? next - uint32_t(keep) : 0) & ~uint32_t(Lz4Window - 1);
                _fast.shift(delta);
                if (_mid) {
                    _mid->shift(delta);
                }
                if (_hc) {
                    _hc->shift(delta);
                }
                next -= delta;
            }
            return next;
        }

        // Compresses [src, src + n), indexes counted from base; matches
        // reach back to low
        SGCL_INLINE_HOT uint8_t* compress(const uint8_t* base, const uint8_t* low, const uint8_t* src, size_t n, uint8_t* dst) noexcept {
            const uint32_t end = uint32_t(src + n - base);
            if (end > next) {
                next = end;
            }
            if (_level < 2) {
                return _fast.compress(base, low, src, n, dst, _level < 0 ? -_level : 1);
            }
            if (_level == 2) {
                return _mid->compress(base, low, src, n, dst);
            }
            return _hc->compress(base, low, src, n, dst, _level);
        }

        // A dictionary's positions into the tables: [from, to) of base
        void load(const uint8_t* base, const uint8_t* from, const uint8_t* to) noexcept {
            if (_level < 2) {
                _fast.load(base, from, to);
            } else if (_level == 2) {
                _mid->load(base, from, to);
            } else {
                _hc->load(base, from, to);
            }
            const uint32_t end = uint32_t(to - base);
            if (end > next) {
                next = end;
            }
        }

        // The tables of another engine of the same level (the state after a
        // dictionary, put back before each independent block)
        void copy_from(const Lz4Engine& o) noexcept {
            _level = o._level;
            next = o.next;
            if (_level < 2) {
                _fast = o._fast;
            } else if (_level == 2) {
                if (!_mid) {
                    _mid = std::make_unique<Lz4Mid>();
                }
                _mid->copy_from(*o._mid);
            } else {
                if (!_hc) {
                    _hc = std::make_unique<Lz4Hc>();
                }
                _hc->copy_from(*o._hc);
            }
        }

        void clear() noexcept {
            _fast.reset();
            if (_mid) {
                _mid->reset();
            }
            if (_hc) {
                _hc->reset();
            }
            next = 0;
        }

        uint32_t next = 0;

    private:
        int _level = 1;
        Lz4Fast _fast;
        std::unique_ptr<Lz4Mid> _mid;
        std::unique_ptr<Lz4Hc> _hc;
    };

    // The engine of a whole compress in memory, lent by the thread as
    // LentDeflater's is (stream.h): kept from one call to the next, so that
    // a small compress neither allocates nor clears its tables. A call made
    // while it is out gets one of its own.
    class LentLz4Engine {
    public:
        explicit LentLz4Engine(int level) noexcept {
            auto& k = _kept();
            if (k.lent) {
                _own = std::make_unique<Lz4Engine>();
                _engine = _own.get();
            } else {
                if (!k.engine) {
                    k.engine = std::make_unique<Lz4Engine>();
                }
                k.lent = true;
                _kept_by = &k;
                _engine = k.engine.get();
            }
            _engine->set_level(level);
        }

        LentLz4Engine(const LentLz4Engine&) = delete;
        LentLz4Engine& operator=(const LentLz4Engine&) = delete;

        SGCL_INLINE_HOT ~LentLz4Engine() {
            if (_kept_by) {
                _kept_by->lent = false;
            }
        }

        SGCL_INLINE_HOT Lz4Engine* operator->() const noexcept {
            return _engine;
        }

        SGCL_INLINE_HOT Lz4Engine& operator*() const noexcept {
            return *_engine;
        }

        // Room for a block compressed: the thread's, kept from call to
        // call up to 8 MB (a block of 4 MB and its bound), so that a call
        // neither allocates it nor faults its pages in again
        uint8_t* scratch(size_t n) noexcept {
            if (_kept_by && n <= KeepBytes) {
                auto& k = *_kept_by;
                if (k.scratch_size < n) {
                    k.scratch.reset(new uint8_t[n]);
                    k.scratch_size = n;
                }
                return k.scratch.get();
            }
            _own_scratch.reset(new uint8_t[n]);
            return _own_scratch.get();
        }

    private:
        static constexpr size_t KeepBytes = size_t(8) << 20;

        struct Kept {
            std::unique_ptr<Lz4Engine> engine;
            std::unique_ptr<uint8_t[]> scratch;
            size_t scratch_size = 0;
            bool lent = false;
        };

        static Kept& _kept() noexcept {
            thread_local Kept kept;
            return kept;
        }

        Lz4Engine* _engine = nullptr;
        std::unique_ptr<Lz4Engine> _own;
        std::unique_ptr<uint8_t[]> _own_scratch;
        Kept* _kept_by = nullptr;
    };

    // What the frame's writer takes from the options, checked
    struct Lz4Settings {
        int level = 1;
        unsigned block_id = 7;
        bool linked = false;
        bool block_checksum = false;
        bool content_checksum = true;
        uint32_t dictionary_id = 0;
        std::vector<uint8_t> dictionary;   // its last 64 KB
        const char* error = nullptr;

        template<class O>
        static Lz4Settings of(const O& o) noexcept {
            Lz4Settings s;
            s.level = o.level.value();
            s.block_id = unsigned(o.block_size);
            s.linked = o.linked_blocks;
            s.block_checksum = o.block_checksum;
            s.content_checksum = o.content_checksum;
            s.dictionary_id = o.dictionary_id;
            if (s.block_id < 4 || s.block_id > 7) {
                s.error = "a block size of kb64, kb256, mb1 or mb4";
            }
            const size_t n = o.dictionary.size();
            const size_t keep = std::min(n, Lz4Window);
            auto p = reinterpret_cast<const uint8_t*>(o.dictionary.data());
            s.dictionary.assign(p + (n - keep), p + n);
            return s;
        }
    };

    // The frame's header: FLG, BD, the content size when given, the
    // dictionary's id when not 0, the header's checksum
    template<class Out>
    void lz4_frame_header(Out& out, const Lz4Settings& s, const uint64_t* content_size) noexcept {
        uint8_t h[4 + 2 + 8 + 4 + 1];
        size_t k = 0;
        h[k++] = uint8_t(Lz4Magic);
        h[k++] = uint8_t(Lz4Magic >> 8);
        h[k++] = uint8_t(Lz4Magic >> 16);
        h[k++] = uint8_t(Lz4Magic >> 24);
        const size_t descriptor = k;
        h[k++] = uint8_t(0x40 | (s.linked ? 0 : 0x20) | (s.block_checksum ? 0x10 : 0) | (content_size ? 0x08 : 0) |
                         (s.content_checksum ? 0x04 : 0) | (s.dictionary_id ? 0x01 : 0));
        h[k++] = uint8_t(s.block_id << 4);
        if (content_size) {
            for (int i = 0; i < 8; ++i) {
                h[k++] = uint8_t(*content_size >> (8 * i));
            }
        }
        if (s.dictionary_id) {
            for (int i = 0; i < 4; ++i) {
                h[k++] = uint8_t(s.dictionary_id >> (8 * i));
            }
        }
        h[k] = uint8_t(hash::detail::xxh32(h + descriptor, k - descriptor, 0) >> 8);
        ++k;
        append_bytes(out, h, k);
    }

    // One block out: compressed into scratch, or stored when that is no
    // smaller; its size, its bytes, and its checksum when asked
    template<class Out>
    void lz4_put_block(Out& out, Lz4Engine& engine, uint8_t* scratch, const uint8_t* base, const uint8_t* low,
                       const uint8_t* src, size_t n, bool checksum) noexcept {
        const uint8_t* end = engine.compress(base, low, src, n, scratch);
        size_t size = size_t(end - scratch);
        const uint8_t* data = scratch;
        uint32_t header = uint32_t(size);
        if (size >= n) {
            data = src;
            size = n;
            header = uint32_t(n) | Lz4Stored;
        }
        lz4_put32(out, header);
        append_bytes(out, data, size);
        if (checksum) {
            lz4_put32(out, hash::detail::xxh32(data, size, 0));
        }
    }

    // The frame's writer: what is written gathers in a window of the
    // history (64 KB) and a block, and goes out a block at a time; with a
    // dictionary, its last 64 KB stay before every independent block, and
    // the tables as they were after it are put back before each
    class Lz4FrameEncoder {
    public:
        static constexpr const char* name = "lz4";

        template<class O>
        explicit Lz4FrameEncoder(const O& o) noexcept
        : _s(Lz4Settings::of(o)) {
            if (_s.error) {
                return;
            }
            if (_s.level < -65537 || _s.level > 12) {
                _s.error = "a level of -65537..12";
                return;
            }
            _block = lz4_block_bytes(_s.block_id);
            _window.reset(new uint8_t[Lz4Window + _block]);
            _scratch.reset(new uint8_t[lz4_bound(_block) + Lz4OutSlack]);
            _engine.set_level(_s.level);
            reset();
        }

        SGCL_INLINE_HOT const char* setup_error() const noexcept {
            return _s.error;
        }

        template<class Out>
        void start(Out& out, const uint64_t* content_size = nullptr) noexcept {
            lz4_frame_header(out, _s, content_size);
        }

        template<class Out>
        void write(const uint8_t* p, size_t n, Out& out) noexcept {
            if (_s.content_checksum) {
                _content.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
            }
            while (n) {
                const size_t k = std::min(n, _block - _fill);
                sgcl::detail::copy_bytes(_window.get() + Lz4Window + _fill, p, k);
                _fill += k;
                p += k;
                n -= k;
                if (_fill == _block) {
                    _emit(out);
                }
            }
        }

        template<class Out>
        void flush(Out& out) noexcept {
            if (_fill) {
                _emit(out);
            }
        }

        template<class Out>
        void finish(Out& out) noexcept {
            flush(out);
            lz4_put32(out, 0);
            if (_s.content_checksum) {
                lz4_put32(out, _content.value());
            }
        }

        void reset() noexcept {
            if (_s.error) {
                return;
            }
            _fill = 0;
            _history = 0;
            _content.reset();
            const size_t d = _s.dictionary.size();
            if (d) {
                sgcl::detail::copy_bytes(_window.get() + Lz4Window - d, _s.dictionary.data(), d);
                _history = d;
                // the dictionary's positions in the tables, kept for the
                // independent blocks; the block always starts at the same index
                _engine.clear();
                _start = Lz4Window;
                const uint8_t* base = _window.get();
                _engine.load(base, base + Lz4Window - d, base + Lz4Window);
                if (!_s.linked) {
                    _with_dictionary = std::make_unique<Lz4Engine>();
                    _with_dictionary->copy_from(_engine);
                }
            }
        }

    private:
        template<class Out>
        void _emit(Out& out) noexcept {
            uint8_t* const block = _window.get() + Lz4Window;
            if (!_s.linked && _with_dictionary) {
                _engine.copy_from(*_with_dictionary);
            } else if (_s.linked || !_s.dictionary.empty()) {
                _start = _engine.room(_fill, _history);
            } else {
                _start = _engine.room(_fill, 0);
            }
            const uint8_t* base = block - _start;
            const uint8_t* low = _s.linked || !_s.dictionary.empty() ? block - _history : block;
            lz4_put_block(out, _engine, _scratch.get(), base, low, block, _fill, _s.block_checksum);
            if (_s.linked) {
                // the last 64 KB of history and block before the next block
                // (the indexes go on from the block's end: the engine's next)
                const size_t keep = std::min(Lz4Window, _history + _fill);
                std::memmove(_window.get() + Lz4Window - keep, block + _fill - keep, keep);
                _history = keep;
            }
            _fill = 0;
        }

        Lz4Settings _s;
        size_t _block = 0;
        std::unique_ptr<uint8_t[]> _window;    // [history of up to 64 KB | the block]
        std::unique_ptr<uint8_t[]> _scratch;   // a block compressed
        size_t _fill = 0;                       // the block's bytes so far
        size_t _history = 0;                    // the history's bytes before the block
        uint32_t _start = 0;                    // the block's first index
        Lz4Engine _engine;
        std::unique_ptr<Lz4Engine> _with_dictionary;
        hash::xxh32 _content;
    };

    // A whole compress in memory with no dictionary: the blocks straight
    // from the input, its history the input itself
    template<class Out>
    void lz4_compress_frame(Out& out, const Lz4Settings& s, const uint8_t* p, size_t n) noexcept {
        const uint64_t size = n;
        lz4_frame_header(out, s, &size);
        const size_t block = lz4_block_bytes(s.block_id);
        LentLz4Engine engine(s.level);
        uint8_t* const scratch = engine.scratch(lz4_bound(std::min(block, n)) + Lz4OutSlack);
        size_t at = 0;
        while (at < n) {
            const size_t k = std::min(block, n - at);
            const size_t keep = s.linked ? std::min(at, Lz4Window) : 0;
            const uint32_t start = engine->room(k, keep);
            const uint8_t* src = p + at;
            const uint8_t* base = src - start;
            const uint8_t* low = s.linked ? src - keep : src;
            lz4_put_block(out, *engine, scratch, base, low, src, k, s.block_checksum);
            at += k;
        }
        lz4_put32(out, 0);
        if (s.content_checksum) {
            lz4_put32(out, hash::detail::xxh32(p, n, 0));
        }
    }

    // The raw block format in memory: one block, no frame
    inline vector<byte> lz4_compress_raw(const uint8_t* p, size_t n, int level, const std::vector<uint8_t>& dictionary) noexcept {
        LentLz4Engine engine(level);
        uint8_t* const out = engine.scratch(lz4_bound(n) + Lz4OutSlack);
        if (dictionary.empty()) {
            const uint32_t start = engine->room(n, 0);
            const uint8_t* end = engine->compress(p - start, p, p, n, out);
            return to_vector(out, size_t(end - out));
        }
        // the dictionary right before the data, in one buffer
        const size_t d = dictionary.size();
        std::vector<uint8_t> joined(d + n);
        sgcl::detail::copy_bytes(joined.data(), dictionary.data(), d);
        sgcl::detail::copy_bytes(joined.data() + d, p, n);
        const uint32_t start = engine->room(d + n, 0);
        const uint8_t* base = joined.data() - start;
        engine->load(base, joined.data(), joined.data() + d);
        const uint8_t* end = engine->compress(base, joined.data(), joined.data() + d, n, out);
        return to_vector(out, size_t(end - out));
    }

    // The frame's descriptor read from p (FLG onwards, `have` bytes there):
    // its size when whole, 0 when more is needed, or a failure
    struct Lz4Descriptor {
        bool linked = false;
        bool block_checksum = false;
        bool content_checksum = false;
        bool has_size = false;
        bool has_dictionary = false;
        unsigned block_id = 7;
        uint64_t content_size = 0;
        uint32_t dictionary_id = 0;
        size_t size = 0;   // FLG to the header's checksum

        // nullptr: read (size set, or 0 for more); else what is wrong
        const char* read(const uint8_t* p, size_t have, errc& code) noexcept {
            size = 0;
            if (have < 2) {
                return nullptr;
            }
            const uint8_t flg = p[0];
            const uint8_t bd = p[1];
            code = errc::invalid_header;
            if ((flg >> 6) != 1) {
                return "lz4: frame version not 01";
            }
            if (flg & 0x02 || bd & 0x8F) {
                return "lz4: reserved bits set in the frame descriptor";
            }
            block_id = (bd >> 4) & 7;
            if (block_id < 4) {
                return "lz4: a block size id below 4";
            }
            linked = !(flg & 0x20);
            block_checksum = flg & 0x10;
            has_size = flg & 0x08;
            content_checksum = flg & 0x04;
            has_dictionary = flg & 0x01;
            const size_t need = 2 + (has_size ? 8 : 0) + (has_dictionary ? 4 : 0) + 1;
            if (have < need) {
                return nullptr;
            }
            size_t k = 2;
            content_size = 0;
            if (has_size) {
                for (int i = 0; i < 8; ++i) {
                    content_size |= uint64_t(p[k++]) << (8 * i);
                }
            }
            dictionary_id = 0;
            if (has_dictionary) {
                dictionary_id = lz4_le32(p + k);
                k += 4;
            }
            if (uint8_t(hash::detail::xxh32(p, k, 0) >> 8) != p[k]) {
                code = errc::checksum;
                return "lz4: the frame descriptor's checksum does not match";
            }
            size = need;
            return nullptr;
        }
    };

    // Whether a frame's dictionary id and the dictionary given agree: a
    // frame that names one needs one, and the id given, when not 0, must
    // be the frame's
    SGCL_INLINE_HOT const char* lz4_dictionary_check(const Lz4Descriptor& d, bool have_dictionary, uint32_t given_id) noexcept {
        if (d.has_dictionary && !have_dictionary) {
            return "lz4: the frame was made with a dictionary";
        }
        if (d.has_dictionary && given_id && given_id != d.dictionary_id) {
            return "lz4: the frame was made with another dictionary";
        }
        return nullptr;
    }

    // A whole decompress in memory with no dictionary: every frame, each
    // block decoded straight into the result, its history the result
    // itself. The result grows by doubling, up to the limit; a frame's
    // content size, when it gives one, sizes it at once
    inline expected<vector<byte>, error> lz4_decompress_all(const uint8_t* p, size_t n, const limits& l) noexcept {
        auto fail = [](errc code, uint64_t at, const char* text) noexcept {
            return unexpected<error>(error(code, at, string(text)));
        };
        const uint64_t ceiling = l.max_size == UINT64_MAX ? UINT64_MAX : l.max_size + 1;
        vector<byte> result;
        size_t capacity = 0;
        size_t total = 0;
        auto out = [&]() noexcept {
            return reinterpret_cast<uint8_t*>(result.data());
        };
        // room up to `end` bytes (but one past the limit at most), and the
        // decoder's slack after it
        auto grow = [&](uint64_t end) noexcept {
            const uint64_t target = std::min(end, ceiling);
            if (target <= capacity) {
                return;
            }
            const uint64_t c = std::min<uint64_t>(std::max<uint64_t>({target, uint64_t(capacity) * 2, uint64_t(1) << 16}), ceiling);
            capacity = size_t(c);
            sgcl::detail::VectorOverwrite::resize(result, capacity + Lz4OutSlack);   // every byte written before it is returned: resize(total) below
        };
        // decodes a block of up to `block` bytes at total: false and the
        // failure set when it does not
        // (low_at: where the history starts in the result, SIZE_MAX for none)
        auto decode = [&](const uint8_t* data, size_t size, size_t block, size_t low_at, uint64_t at_block) noexcept
            -> optional<unexpected<error>> {
            grow(uint64_t(total) + block);
            uint8_t* dst = out() + total;
            const uint8_t* low = low_at == SIZE_MAX ? dst : out() + low_at;
            const bool limited = capacity < uint64_t(total) + block;
            auto r = lz4_decode_block(data, size, dst, out() + (limited ? capacity : total + block), low);
            if (r.status != Lz4Decoded::ok) {
                if (r.status == Lz4Decoded::overflow && limited) {
                    return fail(errc::too_large, at_block, "lz4: decompressed data past the limit");
                }
                return fail(errc::corrupt, at_block + r.at, lz4_why(r.status));
            }
            total += r.written;
            if (total > l.max_size) {
                return fail(errc::too_large, at_block, "lz4: decompressed data past the limit");
            }
            return nullopt;
        };
        size_t at = 0;
        bool any = false;
        while (at < n || !any) {
            if (n - at < 4) {
                return fail(errc::unexpected_end, n, any ? "lz4: data after the last frame is not a frame" : "lz4: unexpected end before a frame");
            }
            const uint32_t magic = lz4_le32(p + at);
            any = true;
            if (lz4_skippable(magic)) {
                if (n - at < 8) {
                    return fail(errc::unexpected_end, n, "lz4: unexpected end in a skippable frame");
                }
                const uint64_t size = lz4_le32(p + at + 4);
                if (size > n - at - 8) {
                    return fail(errc::unexpected_end, n, "lz4: unexpected end in a skippable frame");
                }
                at += 8 + size_t(size);
                continue;
            }
            if (magic == Lz4LegacyMagic) {
                at += 4;
                while (at < n) {
                    if (n - at < 4) {
                        return fail(errc::unexpected_end, n, "lz4: unexpected end at a block's size");
                    }
                    const uint32_t v = lz4_le32(p + at);
                    if (v == Lz4Magic || v == Lz4LegacyMagic || lz4_skippable(v)) {
                        break;
                    }
                    if (v > lz4_bound(Lz4LegacyBlock)) {
                        return fail(errc::corrupt, at, "lz4: a legacy block larger than its bound");
                    }
                    if (v > n - at - 4) {
                        return fail(errc::unexpected_end, n, "lz4: unexpected end in a block");
                    }
                    if (auto e = decode(p + at + 4, v, Lz4LegacyBlock, SIZE_MAX, at + 4)) {
                        return *e;
                    }
                    at += 4 + v;
                }
                continue;
            }
            if (magic != Lz4Magic) {
                return fail(errc::invalid_header, at, at ? "lz4: data after the last frame is not a frame" : "lz4: not an LZ4 frame");
            }
            Lz4Descriptor d;
            errc code = errc::invalid_header;
            if (const char* wrong = d.read(p + at + 4, n - at - 4, code)) {
                return fail(code, at, wrong);
            }
            if (d.size == 0) {
                return fail(errc::unexpected_end, n, "lz4: unexpected end in a frame header");
            }
            if (d.has_dictionary) {
                return fail(errc::dictionary_required, at, "lz4: the frame was made with a dictionary");
            }
            const size_t block = lz4_block_bytes(d.block_id);
            if (block > l.max_memory) {
                return fail(errc::too_large, at, "lz4: the frame's block size needs more memory than the limit allows");
            }
            if (d.has_size) {
                if (d.content_size > l.max_size || total + d.content_size > l.max_size) {
                    return fail(errc::too_large, at, "lz4: decompressed data past the limit");
                }
                grow(total + d.content_size);
            }
            at += 4 + d.size;
            const size_t frame_start = total;
            for (;;) {
                if (n - at < 4) {
                    return fail(errc::unexpected_end, n, "lz4: unexpected end at a block's size");
                }
                const uint32_t v = lz4_le32(p + at);
                at += 4;
                if (v == 0) {
                    break;
                }
                const bool stored = v & Lz4Stored;
                const size_t size = v & ~Lz4Stored;
                if (size > block) {
                    return fail(errc::corrupt, at - 4, "lz4: a block larger than the frame's block size");
                }
                const size_t want = size + (d.block_checksum ? 4 : 0);
                if (want > n - at) {
                    return fail(errc::unexpected_end, n, "lz4: unexpected end in a block");
                }
                const uint8_t* data = p + at;
                if (d.block_checksum && hash::detail::xxh32(data, size, 0) != lz4_le32(data + size)) {
                    return fail(errc::checksum, at, "lz4: a block's checksum does not match");
                }
                if (stored) {
                    grow(uint64_t(total) + size);
                    if (uint64_t(total) + size > l.max_size) {
                        return fail(errc::too_large, at, "lz4: decompressed data past the limit");
                    }
                    copy_out(out() + total, data, size);
                    total += size;
                } else if (auto e = decode(data, size, block, d.linked ? frame_start : SIZE_MAX, at)) {
                    return *e;
                }
                at += want;
            }
            if (d.content_checksum) {
                if (n - at < 4) {
                    return fail(errc::unexpected_end, n, "lz4: unexpected end at the content checksum");
                }
                if (lz4_le32(p + at) != hash::detail::xxh32(out() + frame_start, total - frame_start, 0)) {
                    return fail(errc::checksum, at, "lz4: the content checksum does not match");
                }
                at += 4;
            }
            if (d.has_size && total - frame_start != d.content_size) {
                return fail(errc::corrupt, at, "lz4: the content differs in size from the frame's header");
            }
        }
        result.resize(total);
        return result;
    }

    // The raw block format in memory: one block into at most max_size
    // bytes, its history the dictionary (put right before it)
    inline expected<vector<byte>, error> lz4_decompress_raw(const uint8_t* p, size_t n, size_t max_size, const std::vector<uint8_t>& dictionary) noexcept {
        auto failed = [](const Lz4DecodeResult& r) noexcept {
            if (r.status == Lz4Decoded::overflow) {
                return unexpected<error>(error(errc::too_large, r.at, string("lz4: the block decodes to more than the size given")));
            }
            return unexpected<error>(error(r.status == Lz4Decoded::truncated ? errc::unexpected_end : errc::corrupt, r.at, string(lz4_why(r.status))));
        };
        // no block makes more than 255 bytes a byte of it (a length byte of
        // 255), so the room needs no more than that, whatever the size given
        if (uint64_t(n) * 256 + 64 < max_size) {
            max_size = n * 256 + 64;
        }
        if (dictionary.empty()) {
            // straight into the result: the room of max_size, cut to what was made
            vector<byte> result;
            sgcl::detail::VectorOverwrite::resize(result, max_size + Lz4OutSlack);   // every byte up to written made by the decoder: resize below
            uint8_t* dst = reinterpret_cast<uint8_t*>(result.data());
            auto r = lz4_decode_block(p, n, dst, dst + max_size, dst);
            if (r.status != Lz4Decoded::ok) {
                return failed(r);
            }
            result.resize(r.written);
            return result;
        }
        const size_t d = dictionary.size();
        std::unique_ptr<uint8_t[]> room(new uint8_t[d + max_size + Lz4OutSlack]);
        sgcl::detail::copy_bytes(room.get(), dictionary.data(), d);
        auto r = lz4_decode_block(p, n, room.get() + d, room.get() + d + max_size, room.get());
        if (r.status != Lz4Decoded::ok) {
            return failed(r);
        }
        return to_vector(room.get() + d, r.written);
    }

    // The streaming decoder: the frames one after another, a block decoded
    // into a window of the history (the dictionary, or the 64 KB before a
    // linked block) and the block, then handed out from there
    class Lz4FrameDecoder {
    public:
        static constexpr const char* name = "lz4";

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        template<class O>
        Lz4FrameDecoder(const O& o, const limits& l) noexcept
        : _max_memory(l.max_memory)
        , _dictionary_id(o.dictionary_id) {
            const size_t n = o.dictionary.size();
            const size_t keep = std::min(n, Lz4Window);
            auto p = reinterpret_cast<const uint8_t*>(o.dictionary.data());
            _dictionary.assign(p + (n - keep), p + n);
            reset();
        }

        void reset() noexcept {
            _stage = Stage::magic;
            _have = 0;
            _frames = 0;
            _taken = 0;
            _out_begin = _out_end = 0;
            _history = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return _taken;
        }

        CodecStatus decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) noexcept {
            for (;;) {
                // what the last block made, handed out first
                if (_out_begin < _out_end) {
                    const size_t k = std::min(_out_end - _out_begin, cap - pos);
                    copy_out(out + pos, _window.get() + _out_begin, k);
                    pos += k;
                    _out_begin += k;
                    if (_out_begin < _out_end) {
                        return CodecStatus::need_room;
                    }
                    _after_block();
                }
                switch (_stage) {
                    case Stage::magic: {
                        if (!_gather(in, end, 4)) {
                            if (final) {
                                if (_have == 0 && _frames > 0) {
                                    return CodecStatus::done;
                                }
                                return _fail(errc::unexpected_end, "lz4: unexpected end before a frame");
                            }
                            return CodecStatus::need_input;
                        }
                        const uint32_t magic = lz4_le32(_head);
                        _have = 0;
                        if (magic == Lz4Magic) {
                            _stage = Stage::descriptor;
                        } else if (lz4_skippable(magic)) {
                            _stage = Stage::skip_size;
                        } else if (magic == Lz4LegacyMagic) {
                            if (auto e = _make_window(Lz4LegacyBlock)) {
                                return *e;
                            }
                            _legacy = true;
                            _desc = Lz4Descriptor {};
                            _history = 0;
                            _stage = Stage::block_size;
                        } else {
                            return _fail(errc::invalid_header, _frames ? "lz4: data after the last frame is not a frame"
                                                                       : "lz4: not an LZ4 frame");
                        }
                        break;
                    }
                    case Stage::skip_size: {
                        if (!_gather(in, end, 4)) {
                            return _more(final, "lz4: unexpected end in a skippable frame");
                        }
                        _skip = lz4_le32(_head);
                        _have = 0;
                        _stage = Stage::skip;
                        break;
                    }
                    case Stage::skip: {
                        const size_t k = size_t(std::min<uint64_t>(_skip, uint64_t(end - in)));
                        in += k;
                        _taken += k;
                        _skip -= k;
                        if (_skip) {
                            return _more(final, "lz4: unexpected end in a skippable frame");
                        }
                        ++_frames;
                        _stage = Stage::magic;
                        break;
                    }
                    case Stage::descriptor: {
                        // the descriptor's size depends on its first byte
                        if (!_gather(in, end, 2)) {
                            return _more(final, "lz4: unexpected end in a frame header");
                        }
                        errc code = errc::invalid_header;
                        const char* wrong = _desc.read(_head, _have, code);
                        if (wrong) {
                            return _fail(code, wrong);
                        }
                        if (_desc.size == 0) {
                            const size_t need = 2 + ((_head[0] & 0x08) ? 8 : 0) + ((_head[0] & 0x01) ? 4 : 0) + 1;
                            if (!_gather(in, end, need)) {
                                return _more(final, "lz4: unexpected end in a frame header");
                            }
                            wrong = _desc.read(_head, _have, code);
                            if (wrong) {
                                return _fail(code, wrong);
                            }
                        }
                        _have = 0;
                        if (const char* d = lz4_dictionary_check(_desc, !_dictionary.empty(), _dictionary_id)) {
                            return _fail(errc::dictionary_required, d);
                        }
                        if (auto e = _make_window(lz4_block_bytes(_desc.block_id))) {
                            return *e;
                        }
                        _legacy = false;
                        _produced = 0;
                        _content.reset();
                        _start_history();
                        _stage = Stage::block_size;
                        break;
                    }
                    case Stage::block_size: {
                        if (_legacy && in == end && _have == 0 && final) {
                            ++_frames;   // the legacy frame ends with the input
                            return CodecStatus::done;
                        }
                        if (!_gather(in, end, 4)) {
                            return _more(final, "lz4: unexpected end at a block's size");
                        }
                        const uint32_t v = lz4_le32(_head);
                        _have = 0;
                        if (_legacy) {
                            if (v == Lz4Magic || v == Lz4LegacyMagic || lz4_skippable(v)) {
                                // another frame begins: its magic was read
                                ++_frames;
                                _head[0] = uint8_t(v);
                                _head[1] = uint8_t(v >> 8);
                                _head[2] = uint8_t(v >> 16);
                                _head[3] = uint8_t(v >> 24);
                                _have = 4;
                                _stage = Stage::magic;
                                break;
                            }
                            if (v > lz4_bound(Lz4LegacyBlock)) {
                                return _fail(errc::corrupt, "lz4: a legacy block larger than its bound");
                            }
                            _block_size = v;
                            _stored = false;
                        } else {
                            if (v == 0) {
                                _stage = _desc.content_checksum ? Stage::content_checksum : Stage::frame_end;
                                break;
                            }
                            _stored = v & Lz4Stored;
                            _block_size = v & ~Lz4Stored;
                            if (_block_size > _block_capacity) {
                                return _fail(errc::corrupt, "lz4: a block larger than the frame's block size");
                            }
                        }
                        _block_have = 0;
                        _block_at = _taken;
                        _stage = Stage::block_data;
                        break;
                    }
                    case Stage::block_data: {
                        const size_t want = _block_size + (_desc.block_checksum ? 4 : 0);
                        const uint8_t* data;
                        if (_block_have == 0 && size_t(end - in) >= want) {
                            data = in;   // the whole block is in the input
                            in += want;
                            _taken += want;
                        } else {
                            const size_t k = std::min(want - _block_have, size_t(end - in));
                            sgcl::detail::copy_bytes(_block.get() + _block_have, in, k);
                            in += k;
                            _taken += k;
                            _block_have += k;
                            if (_block_have < want) {
                                return _more(final, "lz4: unexpected end in a block");
                            }
                            data = _block.get();
                        }
                        if (_desc.block_checksum && hash::detail::xxh32(data, _block_size, 0) != lz4_le32(data + _block_size)) {
                            return _fail(errc::checksum, "lz4: a block's checksum does not match");
                        }
                        if (auto e = _decode_block(data)) {
                            return *e;
                        }
                        _stage = Stage::block_size;
                        break;
                    }
                    case Stage::content_checksum: {
                        if (!_gather(in, end, 4)) {
                            return _more(final, "lz4: unexpected end at the content checksum");
                        }
                        _have = 0;
                        if (lz4_le32(_head) != _content.value()) {
                            return _fail(errc::checksum, "lz4: the content checksum does not match");
                        }
                        _stage = Stage::frame_end;
                        break;
                    }
                    case Stage::frame_end: {
                        if (_desc.has_size && _produced != _desc.content_size) {
                            return _fail(errc::corrupt, "lz4: the content differs in size from the frame's header");
                        }
                        ++_frames;
                        _stage = Stage::magic;
                        break;
                    }
                }
            }
        }

    private:
        enum class Stage : uint8_t {
            magic,
            skip_size,
            skip,
            descriptor,
            block_size,
            block_data,
            content_checksum,
            frame_end
        };

        // Up to n bytes of a header gathered in _head; true when there
        SGCL_INLINE_HOT bool _gather(const uint8_t*& in, const uint8_t* end, size_t n) noexcept {
            const size_t k = std::min(n > _have ? n - _have : 0, size_t(end - in));
            sgcl::detail::copy_bytes(_head + _have, in, k);
            _have += k;
            in += k;
            _taken += k;
            return _have >= n;
        }

        SGCL_INLINE_HOT CodecStatus _more(bool final, const char* text) noexcept {
            if (final) {
                return _fail(errc::unexpected_end, text);
            }
            return CodecStatus::need_input;
        }

        CodecStatus _fail(errc code, const char* text) noexcept {
            error = code;
            error_text = text;
            return CodecStatus::failed;
        }

        // The window for blocks of up to n bytes: [64 KB | n | slack]
        optional<CodecStatus> _make_window(size_t n) noexcept {
            if (n > _max_memory) {
                return _fail(errc::too_large, "lz4: the frame's block size needs more memory than the limit allows");
            }
            if (_block_capacity != n) {
                _window.reset(new uint8_t[Lz4Window + n + Lz4OutSlack]);
                _block.reset(new uint8_t[lz4_bound(n) + 4]);
                _block_capacity = n;
                _history = 0;
            }
            return nullopt;
        }

        // The history a frame's first block sees: the dictionary
        SGCL_INLINE_HOT void _start_history() noexcept {
            const size_t d = _dictionary.size();
            if (d) {
                sgcl::detail::copy_bytes(_window.get() + Lz4Window - d, _dictionary.data(), d);
            }
            _history = d;
        }

        optional<CodecStatus> _decode_block(const uint8_t* data) noexcept {
            uint8_t* const block = _window.get() + Lz4Window;
            size_t made;
            if (_stored) {
                sgcl::detail::copy_bytes(block, data, _block_size);
                made = _block_size;
            } else {
                const uint8_t* low = block - _history;
                auto r = lz4_decode_block(data, _block_size, block, block + _block_capacity, low);
                if (r.status != Lz4Decoded::ok) {
                    _taken = _block_at + r.at;
                    return _fail(errc::corrupt, lz4_why(r.status));
                }
                made = r.written;
            }
            if (!_legacy) {
                if (_desc.content_checksum) {
                    _content.update(slice<const byte>(reinterpret_cast<const byte*>(block), made));
                }
                _produced += made;
                if (_desc.has_size && _produced > _desc.content_size) {
                    return _fail(errc::corrupt, "lz4: the content is longer than the frame's header says");
                }
            }
            _out_begin = Lz4Window;
            _out_end = Lz4Window + made;
            return nullopt;
        }

        // After a block went out: the history the next block sees
        void _after_block() noexcept {
            const size_t made = _out_end - Lz4Window;
            if (!_legacy && _desc.linked) {
                const size_t keep = std::min(Lz4Window, _history + made);
                std::memmove(_window.get() + Lz4Window - keep, _window.get() + Lz4Window + made - keep, keep);
                _history = keep;
            }
            _out_begin = _out_end = 0;
        }

        uint64_t _max_memory;
        uint32_t _dictionary_id;
        std::vector<uint8_t> _dictionary;
        Stage _stage = Stage::magic;
        uint8_t _head[20];
        size_t _have = 0;
        Lz4Descriptor _desc;
        bool _legacy = false;
        bool _stored = false;
        uint64_t _skip = 0;
        uint64_t _frames = 0;
        uint64_t _taken = 0;
        uint64_t _produced = 0;
        size_t _block_size = 0;
        size_t _block_have = 0;
        uint64_t _block_at = 0;   // the block's first byte in the input
        size_t _block_capacity = 0;
        std::unique_ptr<uint8_t[]> _window;
        std::unique_ptr<uint8_t[]> _block;
        size_t _history = 0;
        size_t _out_begin = 0;
        size_t _out_end = 0;
        hash::xxh32 _content;
    };
}
