//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "zstd_decode.h"
#include "zstd_entropy.h"

#include <algorithm>
#include <cstdint>
#include <vector>

// Zstandard's block encoder: a block's literals and sequences, found by a
// match finder (zstd_match.h), written as RFC 8878 lays them out.
//
// The literals go raw, as one byte repeated, or Huffman-coded (a table of
// their own, or the one before when that costs less; one stream under 256
// bytes, four from there), whichever is smallest. Each sequence comes with
// its offset's value, which the match finder took against the three offsets
// used last as the decoder will track them (a repeat 1..3, shifted when the
// literals' length is 0), and each of the three codes gets the cheapest mode
// of four:
// the predefined table, one symbol (RLE), a table of the block's own counts,
// or the table before; the costs are estimated from the tables' states.
// The bitstream goes backwards: the last sequence starts the states, the
// others follow from the end, so the decoder reads them in order.
namespace sgcl::compress::detail {
    struct ZstdSequence {
        uint32_t literals;   // literals before the match
        uint32_t length;     // the match, 3 bytes or more
        uint32_t value;      // the offset as the block names it: 1..3 a repeat, past 3 the distance + 3
    };

    // Where a parse puts what it finds, into room the caller made: the
    // sequences (at most one every three bytes, and one more) and the
    // literals (the block's bytes at most, and ZstdOutSlack)
    struct ZstdSink {
        ZstdSequence* seq;
        uint8_t* lit;

        // the literals [anchor, ip), then a match of `length` named by `value`
        SGCL_INLINE_HOT void match(const uint8_t* anchor, const uint8_t* ip, uint32_t value, size_t length) noexcept {
            const size_t ll = size_t(ip - anchor);
            sgcl::detail::copy_bytes(lit, anchor, ll);
            lit += ll;
            *seq++ = {uint32_t(ll), uint32_t(length), value};
        }

        // the literals at the end of the block
        SGCL_INLINE_HOT void rest(const uint8_t* anchor, const uint8_t* iend) noexcept {
            const size_t ll = size_t(iend - anchor);
            sgcl::detail::copy_bytes(lit, anchor, ll);
            lit += ll;
        }
    };

    // The three last offsets after a match `o` with `ll` literals before
    // it, as the decoder moves them: a repeat of the first changes nothing,
    // of the second swaps the two, anything else goes in front (with no
    // literals the first cannot be named as a repeat, and comes in front
    // again as a new offset)
    SGCL_INLINE_HOT void zstd_update_rep(uint32_t (&rep)[3], uint32_t o, uint32_t ll) noexcept {
        if (ll && o == rep[0]) {
            return;
        }
        if (o == rep[1]) {
            rep[1] = rep[0];
            rep[0] = o;
            return;
        }
        rep[2] = rep[1];
        rep[1] = rep[0];
        rep[0] = o;
    }

    // The value of a match `o` back after `ll` literals against the three
    // last offsets (a repeat 1..3, shifted when ll is 0, or the distance
    // + 3), the offsets moved on
    SGCL_INLINE_HOT uint32_t zstd_offset_value(uint32_t (&rep)[3], uint32_t o, uint32_t ll) noexcept {
        uint32_t value;
        if (ll) {
            value = o == rep[0] ? 1 : o == rep[1] ? 2 : o == rep[2] ? 3 : o + 3;
        } else {
            value = o == rep[1] ? 1 : o == rep[2] ? 2 : (o == rep[0] - 1 && rep[0] > 1) ? 3 : o + 3;
        }
        zstd_update_rep(rep, o, ll);
        return value;
    }

    // The literal length's code (RFC 8878, 3.1.1.3.2.1.1)
    SGCL_INLINE_HOT unsigned zstd_ll_code(uint32_t ll) noexcept {
        static constexpr uint8_t small[64] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
                                              16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21,
                                              22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23,
                                              24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24};
        return ll < 64 ? small[ll] : unsigned(31 - std::countl_zero(ll)) + 19;
    }

    // The match length's code, from the length less 3
    SGCL_INLINE_HOT unsigned zstd_ml_code(uint32_t mlb) noexcept {
        static constexpr uint8_t small[128] = {
            0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
            32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 36, 36, 37, 37, 37, 37, 38, 38, 38, 38, 38, 38, 38, 38, 39, 39, 39, 39, 39, 39, 39, 39,
            40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
            42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42, 42};
        return mlb < 128 ? small[mlb] : unsigned(31 - std::countl_zero(mlb)) + 36;
    }

    // What one block leaves to the next on the encoder's side, mirroring
    // what the decoder keeps: the three last offsets, and the last table of
    // each kind (whatever its mode) with the counts it came from
    struct ZstdEncodeState {
        uint32_t rep[3] = {1, 4, 8};
        // the literals' last table
        bool huf_valid = false;
        uint8_t huf_lengths[256];
        uint16_t huf_codes[256];
        unsigned huf_max_bits = 0;
        // the sequences' last tables
        struct Table {
            bool valid = false;
            FseEncoder encoder;
        };
        Table ll, of, ml;

        void reset() noexcept {
            rep[0] = 1;
            rep[1] = 4;
            rep[2] = 8;
            huf_valid = false;
            ll.valid = of.valid = ml.valid = false;
        }
    };

    // The encoders of the predefined tables, built once
    struct ZstdDefaultEncoders {
        FseEncoder ll, of, ml;

        ZstdDefaultEncoders() noexcept {
            const ZstdDefaults& d = ZstdDefaults::get();
            ll.build(d.literal.entry, d.literal.log);
            of.build(d.offset.entry, d.offset.log);
            ml.build(d.match.entry, d.match.log);
        }

        static const ZstdDefaultEncoders& get() noexcept {
            static const ZstdDefaultEncoders e;
            return e;
        }
    };

    // A literals table made for a block, kept by the state when the block is
    struct ZstdLiteralTable {
        bool made = false;
        uint8_t lengths[256];
        uint16_t codes[256];
        unsigned max_bits = 0;
    };

    // The literals section into out; its bytes (a table of their own, if
    // they got one, in made)
    inline size_t zstd_write_literals(const uint8_t* lit, size_t n, const ZstdEncodeState& st, ZstdLiteralTable& made, uint8_t* out,
                                      uint8_t* scratch, bool raw_only = false) noexcept {
        made.made = false;
        auto raw = [&]() noexcept -> size_t {
            size_t h;
            if (n < 32) {
                out[0] = uint8_t(n << 3);
                h = 1;
            } else if (n < 4096) {
                out[0] = uint8_t(0x04 | (n << 4));
                out[1] = uint8_t(n >> 4);
                h = 2;
            } else {
                out[0] = uint8_t(0x0C | (n << 4));
                out[1] = uint8_t(n >> 4);
                out[2] = uint8_t(n >> 12);
                h = 3;
            }
            sgcl::detail::copy_bytes(out + h, lit, n);
            return h + n;
        };
        if (n == 0) {
            out[0] = 0;
            return 1;
        }
        // one byte repeated
        bool same = true;
        for (size_t i = 1; i < n && same; ++i) {
            same = lit[i] == lit[0];
        }
        if (same && n > 2) {
            size_t h;
            if (n < 32) {
                out[0] = uint8_t(1 | (n << 3));
                h = 1;
            } else if (n < 4096) {
                out[0] = uint8_t(0x05 | (n << 4));
                out[1] = uint8_t(n >> 4);
                h = 2;
            } else {
                out[0] = uint8_t(0x0D | (n << 4));
                out[1] = uint8_t(n >> 4);
                out[2] = uint8_t(n >> 12);
                h = 3;
            }
            out[h] = lit[0];
            return h + 1;
        }
        if (n < 64 || raw_only) {
            return raw();
        }
        // four counts side by side: a run of one byte does not wait on its
        // own count
        uint32_t lanes[4][256] = {};
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            ++lanes[0][lit[i]];
            ++lanes[1][lit[i + 1]];
            ++lanes[2][lit[i + 2]];
            ++lanes[3][lit[i + 3]];
        }
        for (; i < n; ++i) {
            ++lanes[0][lit[i]];
        }
        uint32_t counts[256];
        unsigned symbols = 0;
        for (unsigned s = 0; s < 256; ++s) {
            counts[s] = lanes[0][s] + lanes[1][s] + lanes[2][s] + lanes[3][s];
            if (counts[s]) {
                symbols = s + 1;
            }
        }
        // a table of their own, or the one before (when it codes every byte here)
        uint8_t* const lengths = made.lengths;
        uint16_t* const codes = made.codes;
        const unsigned max_bits = huf_lengths(counts, symbols, 11, lengths);
        if (!max_bits) {
            return raw();
        }
        huf_codes(lengths, 256, max_bits, codes);
        uint8_t table[256];
        const size_t table_size = huf_write_table(lengths, 256, max_bits, table);
        if (!table_size) {
            return raw();
        }
        uint64_t own_bits = 0;
        uint64_t old_bits = UINT64_MAX;
        bool old_fits = st.huf_valid;
        for (unsigned s = 0; s < symbols; ++s) {
            own_bits += uint64_t(counts[s]) * lengths[s];
            if (counts[s] && (!st.huf_valid || !st.huf_lengths[s])) {
                old_fits = false;
            }
        }
        if (old_fits) {
            old_bits = 0;
            for (unsigned s = 0; s < symbols; ++s) {
                old_bits += uint64_t(counts[s]) * st.huf_lengths[s];
            }
        }
        const bool reuse = old_fits && old_bits / 8 <= own_bits / 8 + table_size;
        const uint8_t* use_lengths = reuse ? st.huf_lengths : lengths;
        const uint16_t* use_codes = reuse ? st.huf_codes : codes;
        const bool four = n >= 256;
        // the streams into scratch
        size_t streams;
        if (!four) {
            streams = huf_encode_stream(lit, n, use_lengths, use_codes, scratch);
        } else {
            const size_t q = (n + 3) / 4;
            size_t at = 6;
            size_t sizes[4];
            for (int i = 0; i < 4; ++i) {
                const size_t from = size_t(i) * q;
                const size_t count = i < 3 ? q : n - 3 * q;
                sizes[i] = huf_encode_stream(lit + from, count, use_lengths, use_codes, scratch + at);
                at += sizes[i];
            }
            if (sizes[0] > 0xFFFF || sizes[1] > 0xFFFF || sizes[2] > 0xFFFF) {
                return raw();
            }
            for (int i = 0; i < 3; ++i) {
                scratch[2 * i] = uint8_t(sizes[i]);
                scratch[2 * i + 1] = uint8_t(sizes[i] >> 8);
            }
            streams = at;
        }
        const size_t compressed = streams + (reuse ? 0 : table_size);
        // the header: 3, 4 or 5 bytes by the sizes
        size_t h;
        unsigned format;
        if (n < 1024 && compressed < 1024) {
            h = 3;
            format = four ? 1 : 0;
        } else if (n < 16384 && compressed < 16384) {
            h = 4;
            format = 2;
        } else {
            h = 5;
            format = 3;
        }
        if (!four && format != 0) {
            return raw();   // one stream fits the short header only
        }
        if (h + compressed >= n + (n < 32 ? 1 : n < 4096 ? 2 : 3) - (n >> 6)) {
            return raw();   // not worth it
        }
        const uint64_t type = reuse ? 3 : 2;
        uint64_t head = type | uint64_t(format) << 2;
        if (h == 3) {
            head |= uint64_t(n) << 4 | uint64_t(compressed) << 14;
        } else if (h == 4) {
            head |= uint64_t(n) << 4 | uint64_t(compressed) << 18;
        } else {
            head |= uint64_t(n) << 4 | uint64_t(compressed) << 22;
        }
        for (size_t i = 0; i < h; ++i) {
            out[i] = uint8_t(head >> (8 * i));
        }
        size_t at = h;
        if (!reuse) {
            sgcl::detail::copy_bytes(out + at, table, table_size);
            at += table_size;
            made.made = true;
            made.max_bits = max_bits;
        }
        sgcl::detail::copy_bytes(out + at, scratch, streams);
        return at + streams;
    }

    // The mode of one code: chosen by the estimated cost of the block's
    // symbols under each table, the description's bytes counted
    struct ZstdModeChoice {
        unsigned mode = 0;                      // 0 predefined, 1 RLE, 2 compressed, 3 repeat
        const FseEncoder* encoder = nullptr;    // the table used: the predefined one, the one before, or own
        FseEncoder own;                         // for 1 and 2
        uint8_t rle_symbol = 0;
        uint8_t description[512];               // for 2
        size_t description_size = 0;
    };

    inline void zstd_choose_mode(const uint8_t* codes, size_t n, unsigned max_log, const FseEncoder& predefined, bool predefined_allowed,
                                 const ZstdEncodeState::Table& last, ZstdModeChoice& c) noexcept {
        // four counts side by side: a run of one code does not wait on its
        // own count
        uint32_t lanes[4][64] = {};
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            ++lanes[0][codes[i]];
            ++lanes[1][codes[i + 1]];
            ++lanes[2][codes[i + 2]];
            ++lanes[3][codes[i + 3]];
        }
        for (; i < n; ++i) {
            ++lanes[0][codes[i]];
        }
        uint32_t counts[64];
        unsigned top = 0;
        unsigned distinct = 0;
        for (unsigned s = 0; s < 64; ++s) {
            counts[s] = lanes[0][s] + lanes[1][s] + lanes[2][s] + lanes[3][s];
            if (counts[s]) {
                ++distinct;
                top = s;
            }
        }
        if (distinct == 1 && n > 2) {
            c.mode = 1;
            c.rle_symbol = codes[0];
            FseEntry e[1];
            fse_build_rle(e, codes[0]);
            c.own.build(e, 0);
            c.encoder = &c.own;
            return;
        }
        auto cost = [&](const FseEncoder& e) noexcept -> uint64_t {
            uint64_t bits = 0;
            for (unsigned s = 0; s <= top; ++s) {
                if (counts[s]) {
                    if (!e.count[s]) {
                        return UINT64_MAX;
                    }
                    bits += uint64_t(counts[s]) * e.cost(s);
                }
            }
            return bits / 256;
        };
        const uint64_t predefined_cost = predefined_allowed ? cost(predefined) : UINT64_MAX;
        const uint64_t repeat_cost = last.valid ? cost(last.encoder) : UINT64_MAX;
        // a table of the block's own counts: a symbol of k states costs
        // log - log2(k) bits, as FseEncoder::cost counts it
        uint64_t own_cost = UINT64_MAX;
        FseCounts normalized;
        if (n >= 2) {
            const unsigned log = fse_choose_log(n, top, max_log);
            fse_normalize(counts, top + 1, n, log, normalized);
            uint64_t bits = 0;
            const uint32_t whole = FseEncoder::fse_log2_256(uint32_t(1) << log);
            for (unsigned s = 0; s <= top; ++s) {
                if (counts[s]) {
                    const int k = normalized.count[s];
                    bits += uint64_t(counts[s]) * (whole - FseEncoder::fse_log2_256(uint32_t(k < 1 ? 1 : k)));
                }
            }
            c.description_size = fse_write_counts(normalized, c.description);
            own_cost = bits / 256 + c.description_size * 8;
        }
        if (own_cost < predefined_cost && own_cost < repeat_cost) {
            FseEntry table[512];
            if (fse_build_decoder(normalized, table)) {
                c.mode = 2;
                c.own.build(table, normalized.log);
                c.encoder = &c.own;
                return;
            }
        }
        if (repeat_cost <= predefined_cost) {
            c.mode = 3;
            c.encoder = &last.encoder;
        } else {
            c.mode = 0;
            c.encoder = &predefined;
        }
    }

    // A whole compressed block's body (literals and sequences) into out (a
    // room of its own bound); its bytes. The state's tables move on only
    // when the caller keeps the block (commit)
    struct ZstdBlockWork {
        // a block has at most one sequence every three bytes
        static constexpr size_t MaxSequences = ZstdBlockMax / 3 + 1;
        std::unique_ptr<uint8_t[]> codes {new uint8_t[3 * MaxSequences]};   // ll, of, ml
        std::vector<uint8_t> scratch;
        ZstdLiteralTable literals;
        ZstdModeChoice ll, of, ml;
    };

    inline size_t zstd_write_block(const ZstdSequence* seqs, size_t count, const uint8_t* lit, size_t lit_count, const ZstdEncodeState& st,
                                   ZstdBlockWork& w, uint8_t* out, bool raw_literals = false) noexcept {
        if (w.scratch.size() < ZstdBlockMax + 64) {
            w.scratch.resize(ZstdBlockMax + 64);
        }
        size_t at = zstd_write_literals(lit, lit_count, st, w.literals, out, w.scratch.data(), raw_literals);
        // the number of sequences
        if (count < 128) {
            out[at++] = uint8_t(count);
        } else if (count < 0x7F00) {
            out[at++] = uint8_t(128 + (count >> 8));
            out[at++] = uint8_t(count);
        } else {
            out[at++] = 255;
            out[at++] = uint8_t(count - 0x7F00);
            out[at++] = uint8_t((count - 0x7F00) >> 8);
        }
        if (count == 0) {
            return at;
        }
        // the three codes
        uint8_t* const ll_codes = w.codes.get();
        uint8_t* const of_codes = ll_codes + ZstdBlockWork::MaxSequences;
        uint8_t* const ml_codes = of_codes + ZstdBlockWork::MaxSequences;
        unsigned max_of = 0;
        for (size_t i = 0; i < count; ++i) {
            const ZstdSequence& q = seqs[i];
            const unsigned oc = unsigned(31 - std::countl_zero(q.value));
            of_codes[i] = uint8_t(oc);
            max_of = std::max(max_of, oc);
            ll_codes[i] = uint8_t(zstd_ll_code(q.literals));
            ml_codes[i] = uint8_t(zstd_ml_code(q.length - 3));
        }
        // the modes
        const ZstdDefaultEncoders& d = ZstdDefaultEncoders::get();
        zstd_choose_mode(ll_codes, count, 9, d.ll, true, st.ll, w.ll);
        zstd_choose_mode(of_codes, count, 8, d.of, max_of <= 28, st.of, w.of);
        zstd_choose_mode(ml_codes, count, 9, d.ml, true, st.ml, w.ml);
        out[at++] = uint8_t(w.ll.mode << 6 | w.of.mode << 4 | w.ml.mode << 2);
        auto describe = [&](const ZstdModeChoice& c) noexcept {
            if (c.mode == 1) {
                out[at++] = c.rle_symbol;
            } else if (c.mode == 2) {
                sgcl::detail::copy_bytes(out + at, c.description, c.description_size);
                at += c.description_size;
            }
        };
        describe(w.ll);
        describe(w.of);
        describe(w.ml);
        // the bitstream, from the last sequence to the first
        ZstdBitWriter bw(out + at);
        const FseEncoder& el = *w.ll.encoder;
        const FseEncoder& eo = *w.of.encoder;
        const FseEncoder& em = *w.ml.encoder;
        // each sequence's extra bits: the literals', the match's, the
        // offset's (its value less the code's power of two)
        auto extra = [&](size_t k, uint32_t& lv, unsigned& lb, uint32_t& mv, unsigned& mb, uint32_t& ov, unsigned& ob) noexcept {
            const ZstdSequence& q = seqs[k];
            const unsigned lc = ll_codes[k];
            const unsigned mc = ml_codes[k];
            ob = of_codes[k];
            ov = q.value - (uint32_t(1) << ob);
            lb = ZstdLiteralBits[lc];
            lv = q.literals - ZstdLiteralBase[lc];
            mb = ZstdMatchBits[mc];
            mv = q.length - ZstdMatchBase[mc];
        };
        size_t k = count - 1;
        uint32_t sml = em.start(ml_codes[k]);
        uint32_t sof = eo.start(of_codes[k]);
        uint32_t sll = el.start(ll_codes[k]);
        {
            uint32_t lv, mv, ov;
            unsigned lb, mb, ob;
            extra(k, lv, lb, mv, mb, ov, ob);
            bw.add(lv, int(lb));
            bw.add(mv, int(mb));
            bw.flush();
            bw.add(ov, int(ob));
            bw.flush();
        }
        // the writer holds 64 bits and at most 7 after a flush: the states
        // take 26 at most, so with up to 31 extra bits one flush does a
        // sequence, with up to 57 two, and past that three (a flush always
        // after the states measured slower: the flushes are one chain)
        while (k-- > 0) {
            uint32_t lv, mv, ov;
            unsigned lb, mb, ob;
            extra(k, lv, lb, mv, mb, ov, ob);
            const unsigned extra_bits = lb + mb + ob;
            // the three states' bits joined, and the two lengths' extras
            // joined: fewer steps through the writer's one accumulator
            uint32_t vo, vm, vl;
            const uint32_t no = eo.step(sof, of_codes[k], vo);
            const uint32_t nm = em.step(sml, ml_codes[k], vm);
            const uint32_t nl = el.step(sll, ll_codes[k], vl);
            bw.add(uint64_t(vo) | uint64_t(vm) << no | uint64_t(vl) << (no + nm), int(no + nm + nl));
            if (extra_bits > 31) {
                bw.flush();
            }
            bw.add(uint64_t(lv) | uint64_t(mv) << lb, int(lb + mb));
            if (extra_bits > 57) {
                bw.flush();
            }
            bw.add(ov, int(ob));
            bw.flush();
        }
        em.finish(sml, bw);
        eo.finish(sof, bw);
        bw.flush();
        el.finish(sll, bw);
        at += bw.close();
        return at;
    }

    // After a block was kept: the offsets as the parse left them, the
    // literals' table if it made one, the tables kept for the modes that say
    // "the one before"
    inline void zstd_commit_block(size_t count, const uint32_t (&rep)[3], ZstdEncodeState& st, const ZstdBlockWork& w) noexcept {
        st.rep[0] = rep[0];
        st.rep[1] = rep[1];
        st.rep[2] = rep[2];
        if (w.literals.made) {
            st.huf_valid = true;
            std::copy(std::begin(w.literals.lengths), std::end(w.literals.lengths), st.huf_lengths);
            std::copy(std::begin(w.literals.codes), std::end(w.literals.codes), st.huf_codes);
            st.huf_max_bits = w.literals.max_bits;
        }
        if (count) {
            auto keep = [](ZstdEncodeState::Table& t, const ZstdModeChoice& c) noexcept {
                if (c.encoder != &t.encoder) {
                    t.encoder = *c.encoder;
                }
                t.valid = true;
            };
            keep(st.ll, w.ll);
            keep(st.of, w.of);
            keep(st.ml, w.ml);
        }
    }
}
