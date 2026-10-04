//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

// What a HEIF file must not hand the system's decoder: an HEVC slice
// segment whose entry points (the starts of its tiles or of its rows of
// wavefront-parallel coding) lie at or past the end of the slice's data.
// macOS's VideoToolbox waits for ever on its decoding service on such a
// slice (macOS 26.5, ImageIO through VTTileDecompressionSession: the call
// never returns, the thread blocks in xpc_connection_send_message_with_reply_sync
// at 0% CPU), so the file is refused before ImageIO sees it.
//
// Read from the specifications: the boxes of ISO/IEC 14496-12 and
// 23008-12 (meta, iinf, iloc, iprp, ipco, ipma, idat), the decoder
// configuration of ISO/IEC 14496-15 (hvcC), and of H.265 (ISO/IEC
// 23008-2) the parameter sets and the slice segment header up to its entry
// points (7.3.2.2, 7.3.2.3, 7.3.6.1, 7.3.7). Every hvc1 item of the file is
// read. The guard refuses only what it proves: what it cannot read (a box
// cut short, an extension it does not parse, a P or B slice, an item built
// from other items' data) is left to ImageIO, which judges the file as
// before. Its work is linear in the file: items overlapping past four
// times its size (no encoder writes them) are errc::unsupported.
namespace sgcl::codec::detail {
    inline uint32_t heif_be(const uint8_t* p, unsigned n) noexcept {
        uint32_t v = 0;
        for (unsigned i = 0; i < n; ++i) {
            v = v << 8 | p[i];
        }
        return v;
    }

    inline uint64_t heif_be64(const uint8_t* p, unsigned n) noexcept {
        uint64_t v = 0;
        for (unsigned i = 0; i < n; ++i) {
            v = v << 8 | p[i];
        }
        return v;
    }

    SGCL_INLINE_HOT constexpr uint32_t heif_fourcc(const char (&s)[5]) noexcept {
        return uint32_t(uint8_t(s[0])) << 24 | uint32_t(uint8_t(s[1])) << 16 | uint32_t(uint8_t(s[2])) << 8 | uint8_t(s[3]);
    }

    // A box of ISO/IEC 14496-12 4.2: its type, where its body starts and
    // where it ends
    struct HeifBox {
        uint32_t type = 0;
        size_t body = 0;
        size_t end = 0;
    };

    // The boxes one after another in [at, end); stops at the first that
    // does not fit (a size below its header or past the parent)
    class HeifBoxes {
    public:
        SGCL_INLINE_HOT HeifBoxes(const uint8_t* p, size_t at, size_t end) noexcept
        : _p(p), _at(at), _end(end) {
        }

        bool next(HeifBox& b) noexcept {
            if (_at > _end || _end - _at < 8) {
                return false;
            }
            uint64_t size = heif_be(_p + _at, 4);
            size_t header = 8;
            if (size == 1) {
                if (_end - _at < 16) {
                    return false;
                }
                size = heif_be64(_p + _at + 8, 8);
                header = 16;
            } else if (size == 0) {
                size = _end - _at;
            }
            if (size < header || size > _end - _at) {
                return false;
            }
            b.type = heif_be(_p + _at + 4, 4);
            b.body = _at + header;
            b.end = _at + size_t(size);
            _at = b.end;
            return true;
        }

    private:
        const uint8_t* _p;
        size_t _at;
        size_t _end;
    };

    // The bits of an HEVC NAL unit's payload (after its two-byte header),
    // emulation prevention bytes skipped (H.265 7.3.1.1), with the offset
    // of the next byte in the NAL unit itself, prevention bytes counted
    class HevcBits {
    public:
        SGCL_INLINE_HOT HevcBits(const uint8_t* nal, size_t size) noexcept
        : _p(nal), _n(size), _at(size < 2 ? size : 2) {
        }

        uint32_t bit() noexcept {
            if (_left == 0) {
                if (_zeros >= 2 && _at < _n && _p[_at] == 3) {
                    ++_at;
                    _zeros = 0;
                }
                if (_at >= _n) {
                    _over = true;
                    return 0;
                }
                _byte = _p[_at++];
                _zeros = _byte == 0 ? _zeros + 1 : 0;
                _left = 8;
            }
            --_left;
            return (_byte >> _left) & 1;
        }

        uint32_t bits(unsigned n) noexcept {
            uint32_t v = 0;
            for (unsigned i = 0; i < n; ++i) {
                v = v << 1 | bit();
            }
            return v;
        }

        // ue(v); a code of more than 31 leading zeros is no value of the
        // syntax and stops the reading
        uint32_t ue() noexcept {
            unsigned zeros = 0;
            while (bit() == 0) {
                if (_over || ++zeros > 31) {
                    _over = true;
                    return 0;
                }
            }
            return uint32_t((uint64_t(1) << zeros) - 1 + bits(zeros));
        }

        SGCL_INLINE_HOT int32_t se() noexcept {
            const uint32_t k = ue();
            return k & 1 ? int32_t((k >> 1) + 1) : -int32_t(k >> 1);
        }

        // byte_alignment(): the rest of the byte the reading stands in
        SGCL_INLINE_HOT void align() noexcept {
            _left = 0;
        }

        // The reading went past the end of the unit, or met a code no
        // value of the syntax has
        SGCL_INLINE_HOT bool over() const noexcept {
            return _over;
        }

        void fail() noexcept {
            _over = true;
        }

        // The offset in the unit of the next byte not yet read
        SGCL_INLINE_HOT size_t offset() const noexcept {
            return _at;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _n;
        }

    private:
        const uint8_t* _p;
        size_t _n;
        size_t _at;
        unsigned _zeros = 0;
        unsigned _left = 0;
        uint8_t _byte = 0;
        bool _over = false;
    };

    // Ceil(Log2(n)) of the syntax's u(v) lengths
    inline unsigned hevc_ceil_log2(uint32_t n) noexcept {
        unsigned k = 0;
        while (k < 32 && (uint64_t(1) << k) < n) {
            ++k;
        }
        return k;
    }

    // What a slice segment header needs of a sequence parameter set
    // (7.3.2.2.1), read up to strong_intra_smoothing_enabled_flag
    struct HevcSps {
        bool valid = false;
        bool separate_colour_plane = false;
        uint32_t chroma_format_idc = 0;
        uint32_t pic_size_in_ctbs = 0;
        unsigned log2_max_poc_lsb = 0;
        bool sample_adaptive_offset = false;
        uint32_t num_short_term_ref_pic_sets = 0;
        uint8_t num_delta_pocs[64] = {};
        bool long_term_ref_pics_present = false;
        uint32_t num_long_term_ref_pics_sps = 0;
        bool temporal_mvp = false;
    };

    // What a slice segment header needs of a picture parameter set
    // (7.3.2.3.1, with its range and screen content extensions); valid
    // only when it was read to the end of what the header needs
    struct HevcPps {
        bool valid = false;
        uint32_t sps_id = 0;
        bool dependent_slice_segments = false;
        bool output_flag_present = false;
        unsigned num_extra_slice_header_bits = 0;
        bool slice_chroma_qp_offsets_present = false;
        bool tiles = false;
        bool entropy_coding_sync = false;
        bool loop_filter_across_slices = false;
        bool deblocking_override = false;
        bool deblocking_disabled = false;
        bool slice_header_extension = false;
        bool chroma_qp_offset_list = false;
        bool slice_act_qp_offsets = false;
    };

    // profile_tier_level(1, max_sub_layers_minus1) (7.3.3): skipped
    inline void hevc_skip_ptl(HevcBits& r, unsigned max_sub_layers_minus1) noexcept {
        r.bits(8);   // general_profile_space, tier, idc
        r.bits(32);  // general_profile_compatibility_flag
        r.bits(32);  // the four source flags and 43 bits + 1 of constraints...
        r.bits(16);  // ...48 in all
        r.bits(8);   // general_level_idc
        bool profile[8] = {}, level[8] = {};
        for (unsigned i = 0; i < max_sub_layers_minus1; ++i) {
            profile[i] = r.bit();
            level[i] = r.bit();
        }
        if (max_sub_layers_minus1 > 0) {
            for (unsigned i = max_sub_layers_minus1; i < 8; ++i) {
                r.bits(2);
            }
        }
        for (unsigned i = 0; i < max_sub_layers_minus1; ++i) {
            if (profile[i]) {
                r.bits(32);
                r.bits(32);
                r.bits(24);   // 88 bits
            }
            if (level[i]) {
                r.bits(8);
            }
        }
    }

    // scaling_list_data() (7.3.4): skipped
    inline void hevc_skip_scaling_list(HevcBits& r) noexcept {
        for (unsigned size_id = 0; size_id < 4 && !r.over(); ++size_id) {
            for (unsigned matrix_id = 0; matrix_id < 6; matrix_id += size_id == 3 ? 3 : 1) {
                if (!r.bit()) {
                    r.ue();   // scaling_list_pred_matrix_id_delta
                } else {
                    const unsigned coefs = std::min(64u, 1u << (4 + (size_id << 1)));
                    if (size_id > 1) {
                        r.se();
                    }
                    for (unsigned i = 0; i < coefs && !r.over(); ++i) {
                        r.se();
                    }
                }
            }
        }
    }

    // The delta POCs of the short-term sets of an SPS, as 7.4.8 derives
    // them: what a later set predicted from one needs
    struct HevcRps {
        uint8_t negative = 0;
        uint8_t positive = 0;
        int32_t s0[16] = {};
        int32_t s1[16] = {};
    };

    // st_ref_pic_set(idx) (7.3.7) and its derivation (7.4.8); sets holds
    // the sets 0..num-1 of the SPS. False when it breaks a range of the
    // syntax (more than 16 pictures on a side)
    inline bool hevc_st_ref_pic_set(HevcBits& r, uint32_t idx, uint32_t num, const HevcRps* sets, HevcRps& out) noexcept {
        out = HevcRps{};
        const bool inter = idx != 0 && r.bit();
        if (inter) {
            uint32_t delta_idx = 1;
            if (idx == num) {
                delta_idx = r.ue() + 1;
            }
            if (delta_idx > idx) {
                return false;
            }
            const int sign = int(r.bit());
            const uint32_t abs_minus1 = r.ue();
            if (abs_minus1 > 32767) {
                return false;
            }
            const int32_t delta_rps = (1 - 2 * sign) * int32_t(abs_minus1 + 1);
            const HevcRps& ref = sets[idx - delta_idx];
            const unsigned n = unsigned(ref.negative) + ref.positive;
            bool used[33], use_delta[33];
            for (unsigned j = 0; j <= n; ++j) {
                used[j] = r.bit();
                use_delta[j] = used[j] ? true : bool(r.bit());
            }
            unsigned i = 0;
            for (int j = int(ref.positive) - 1; j >= 0; --j) {
                const int32_t d = ref.s1[j] + delta_rps;
                if (d < 0 && use_delta[ref.negative + j]) {
                    if (i == 16) {
                        return false;
                    }
                    out.s0[i++] = d;
                }
            }
            if (delta_rps < 0 && use_delta[n]) {
                if (i == 16) {
                    return false;
                }
                out.s0[i++] = delta_rps;
            }
            for (unsigned j = 0; j < ref.negative; ++j) {
                const int32_t d = ref.s0[j] + delta_rps;
                if (d < 0 && use_delta[j]) {
                    if (i == 16) {
                        return false;
                    }
                    out.s0[i++] = d;
                }
            }
            out.negative = uint8_t(i);
            i = 0;
            for (int j = int(ref.negative) - 1; j >= 0; --j) {
                const int32_t d = ref.s0[j] + delta_rps;
                if (d > 0 && use_delta[j]) {
                    if (i == 16) {
                        return false;
                    }
                    out.s1[i++] = d;
                }
            }
            if (delta_rps > 0 && use_delta[n]) {
                if (i == 16) {
                    return false;
                }
                out.s1[i++] = delta_rps;
            }
            for (unsigned j = 0; j < ref.positive; ++j) {
                const int32_t d = ref.s1[j] + delta_rps;
                if (d > 0 && use_delta[ref.negative + j]) {
                    if (i == 16) {
                        return false;
                    }
                    out.s1[i++] = d;
                }
            }
            out.positive = uint8_t(i);
            return !r.over();
        }
        const uint32_t negative = r.ue(), positive = r.ue();
        if (negative > 16 || positive > 16 || negative + positive > 16) {
            return false;
        }
        int32_t poc = 0;
        for (uint32_t i = 0; i < negative; ++i) {
            const uint32_t d = r.ue();
            if (d > 32767) {
                return false;
            }
            poc -= int32_t(d + 1);
            out.s0[i] = poc;
            r.bit();
        }
        poc = 0;
        for (uint32_t i = 0; i < positive; ++i) {
            const uint32_t d = r.ue();
            if (d > 32767) {
                return false;
            }
            poc += int32_t(d + 1);
            out.s1[i] = poc;
            r.bit();
        }
        out.negative = uint8_t(negative);
        out.positive = uint8_t(positive);
        return !r.over();
    }

    // st_ref_pic_set(num_short_term_ref_pic_sets) of a slice header read
    // past: a set predicted from one of the SPS needs only that set's count
    inline bool hevc_skip_st_ref_pic_set(HevcBits& r, const HevcSps& s) noexcept {
        const uint32_t num = s.num_short_term_ref_pic_sets;
        if (num != 0 && r.bit()) {   // inter_ref_pic_set_prediction_flag
            const uint32_t delta_idx = r.ue() + 1;
            if (delta_idx > num) {
                return false;
            }
            r.bit();
            r.ue();   // delta_rps_sign, abs_delta_rps_minus1
            for (unsigned j = 0; j <= s.num_delta_pocs[num - delta_idx]; ++j) {
                if (!r.bit()) {   // used_by_curr_pic_flag
                    r.bit();      // use_delta_flag
                }
            }
            return !r.over();
        }
        const uint32_t negative = r.ue(), positive = r.ue();
        if (negative > 16 || positive > 16 || negative + positive > 16) {
            return false;
        }
        for (uint32_t i = 0; i < negative + positive; ++i) {
            r.ue();
            r.bit();
        }
        return !r.over();
    }

    // seq_parameter_set_rbsp() of the base layer up to what a slice header
    // needs; the id it has, or -1 when it cannot be read
    inline int hevc_read_sps(const uint8_t* nal, size_t size, HevcSps& s) {
        HevcBits r(nal, size);
        s = HevcSps{};
        r.bits(4);   // sps_video_parameter_set_id
        const unsigned max_sub_layers_minus1 = r.bits(3);
        r.bit();
        if (max_sub_layers_minus1 > 6) {
            return -1;
        }
        hevc_skip_ptl(r, max_sub_layers_minus1);
        const uint32_t id = r.ue();
        if (id > 15) {
            return -1;
        }
        s.chroma_format_idc = r.ue();
        if (s.chroma_format_idc > 3) {
            return -1;
        }
        if (s.chroma_format_idc == 3) {
            s.separate_colour_plane = r.bit();
        }
        const uint32_t width = r.ue(), height = r.ue();
        if (r.bit()) {
            r.ue(), r.ue(), r.ue(), r.ue();
        }
        r.ue(), r.ue();   // bit depths
        const uint32_t log2_poc_minus4 = r.ue();
        if (log2_poc_minus4 > 12) {
            return -1;
        }
        s.log2_max_poc_lsb = log2_poc_minus4 + 4;
        const bool ordering = r.bit();
        for (unsigned i = ordering ? 0 : max_sub_layers_minus1; i <= max_sub_layers_minus1; ++i) {
            r.ue(), r.ue(), r.ue();
        }
        const uint32_t log2_min_cb_minus3 = r.ue(), log2_diff_cb = r.ue();
        if (log2_min_cb_minus3 > 3 || log2_diff_cb > 3 || log2_min_cb_minus3 + log2_diff_cb > 3) {
            return -1;   // a CTB of 8 to 64
        }
        const unsigned log2_ctb = log2_min_cb_minus3 + 3 + log2_diff_cb;
        const uint64_t ctbs = ((uint64_t(width) + (1u << log2_ctb) - 1) >> log2_ctb) * ((uint64_t(height) + (1u << log2_ctb) - 1) >> log2_ctb);
        if (ctbs == 0 || ctbs > 0xFFFFFFFFu) {
            return -1;
        }
        s.pic_size_in_ctbs = uint32_t(ctbs);
        r.ue(), r.ue(), r.ue(), r.ue();   // transform block sizes, hierarchy depths
        if (r.bit() && r.bit()) {
            hevc_skip_scaling_list(r);
        }
        r.bit();   // amp_enabled_flag
        s.sample_adaptive_offset = r.bit();
        if (r.bit()) {   // pcm_enabled_flag
            r.bits(8);
            r.ue(), r.ue();
            r.bit();
        }
        s.num_short_term_ref_pic_sets = r.ue();
        if (s.num_short_term_ref_pic_sets > 64 || r.over()) {
            return -1;
        }
        std::vector<HevcRps> sets(s.num_short_term_ref_pic_sets + 1);
        for (uint32_t i = 0; i < s.num_short_term_ref_pic_sets; ++i) {
            if (!hevc_st_ref_pic_set(r, i, s.num_short_term_ref_pic_sets, sets.data(), sets[i])) {
                return -1;
            }
            s.num_delta_pocs[i] = uint8_t(sets[i].negative + sets[i].positive);
        }
        s.long_term_ref_pics_present = r.bit();
        if (s.long_term_ref_pics_present) {
            s.num_long_term_ref_pics_sps = r.ue();
            if (s.num_long_term_ref_pics_sps > 32) {
                return -1;
            }
            for (uint32_t i = 0; i < s.num_long_term_ref_pics_sps; ++i) {
                r.bits(s.log2_max_poc_lsb);
                r.bit();
            }
        }
        s.temporal_mvp = r.bit();
        r.bit();   // strong_intra_smoothing_enabled_flag
        if (r.over()) {
            return -1;
        }
        s.valid = true;
        return int(id);
    }

    // pic_parameter_set_rbsp() up to what a slice header needs; the id it
    // has, or -1 when it cannot be read (a multilayer or 3D extension,
    // which may change what the screen content one holds, among them)
    inline int hevc_read_pps(const uint8_t* nal, size_t size, HevcPps& p) noexcept {
        HevcBits r(nal, size);
        p = HevcPps{};
        const uint32_t id = r.ue();
        p.sps_id = r.ue();
        if (id > 63 || p.sps_id > 15) {
            return -1;
        }
        p.dependent_slice_segments = r.bit();
        p.output_flag_present = r.bit();
        p.num_extra_slice_header_bits = r.bits(3);
        r.bit(), r.bit();   // sign_data_hiding, cabac_init_present
        r.ue(), r.ue();     // num_ref_idx_l0/l1_default_active_minus1
        r.se();             // init_qp_minus26
        r.bit();            // constrained_intra_pred_flag
        const bool transform_skip = r.bit();
        if (r.bit()) {      // cu_qp_delta_enabled_flag
            r.ue();
        }
        r.se(), r.se();     // pps_cb_qp_offset, pps_cr_qp_offset
        p.slice_chroma_qp_offsets_present = r.bit();
        r.bit(), r.bit(), r.bit();   // weighted_pred, weighted_bipred, transquant_bypass
        p.tiles = r.bit();
        p.entropy_coding_sync = r.bit();
        if (p.tiles) {
            const uint32_t columns = r.ue(), rows = r.ue();
            if (columns > 1024 || rows > 1024) {
                return -1;
            }
            if (!r.bit()) {   // uniform_spacing_flag
                for (uint32_t i = 0; i < columns + rows && !r.over(); ++i) {
                    r.ue();
                }
            }
            r.bit();   // loop_filter_across_tiles_enabled_flag
        }
        p.loop_filter_across_slices = r.bit();
        if (r.bit()) {   // deblocking_filter_control_present_flag
            p.deblocking_override = r.bit();
            p.deblocking_disabled = r.bit();
            if (!p.deblocking_disabled) {
                r.se(), r.se();
            }
        }
        if (r.bit()) {   // pps_scaling_list_data_present_flag
            hevc_skip_scaling_list(r);
        }
        r.bit();   // lists_modification_present_flag
        r.ue();    // log2_parallel_merge_level_minus2
        p.slice_header_extension = r.bit();
        if (r.bit()) {   // pps_extension_present_flag
            const bool range = r.bit(), multilayer = r.bit(), three_d = r.bit(), scc = r.bit();
            r.bits(4);
            if (multilayer || three_d) {
                return -1;
            }
            if (range) {
                if (transform_skip) {
                    r.ue();
                }
                r.bit();   // cross_component_prediction_enabled_flag
                p.chroma_qp_offset_list = r.bit();
                if (p.chroma_qp_offset_list) {
                    r.ue();
                    const uint32_t len = r.ue();
                    if (len > 5) {
                        return -1;
                    }
                    for (uint32_t i = 0; i <= len; ++i) {
                        r.se(), r.se();
                    }
                }
                r.ue(), r.ue();   // log2_sao_offset_scale_luma, chroma
            }
            if (scc) {
                r.bit();   // pps_curr_pic_ref_enabled_flag
                if (r.bit()) {   // residual_adaptive_colour_transform_enabled_flag
                    p.slice_act_qp_offsets = r.bit();
                }
            }
        }
        if (r.over()) {
            return -1;
        }
        p.valid = true;
        return int(id);
    }

    // The parameter sets an item's slices refer to: its hvcC's, then the
    // ones among its own NAL units
    struct HevcSets {
        std::vector<HevcSps> sps = std::vector<HevcSps>(16);
        std::vector<HevcPps> pps = std::vector<HevcPps>(64);

        void add(const uint8_t* nal, size_t size) {
            if (size < 2 || (nal[0] >> 1 & 63) < 33 || (nal[0] >> 1 & 63) > 34 || ((nal[0] & 1) << 5 | nal[1] >> 3) != 0) {
                return;   // a layer above the base one is not the image's
            }
            if ((nal[0] >> 1 & 63) == 33) {
                HevcSps s;
                if (const int id = hevc_read_sps(nal, size, s); id >= 0) {
                    sps[size_t(id)] = s;
                }
            } else {
                HevcPps p;
                if (const int id = hevc_read_pps(nal, size, p); id >= 0) {
                    pps[size_t(id)] = p;
                }
            }
        }
    };

    // A slice segment's entry points against its data (7.3.6.1, 7.4.7.1:
    // the subsets of the data lie one after another in it, each of at
    // least a byte): true when one starts at or past its end. A header
    // that cannot be read, or one of a P or B slice, is not judged
    inline bool hevc_entry_points_past_end(const uint8_t* nal, size_t size, const HevcSets& sets) noexcept {
        const unsigned type = nal[0] >> 1 & 63;
        HevcBits r(nal, size);
        const bool first = r.bit();
        if (type >= 16 && type <= 23) {
            r.bit();   // no_output_of_prior_pics_flag
        }
        const uint32_t pps_id = r.ue();
        if (pps_id > 63 || !sets.pps[pps_id].valid || !sets.sps[sets.pps[pps_id].sps_id].valid) {
            return false;
        }
        const HevcPps& p = sets.pps[pps_id];
        const HevcSps& s = sets.sps[p.sps_id];
        bool dependent = false;
        if (!first) {
            if (p.dependent_slice_segments) {
                dependent = r.bit();
            }
            r.bits(hevc_ceil_log2(s.pic_size_in_ctbs));   // slice_segment_address
        }
        if (!dependent) {
            r.bits(p.num_extra_slice_header_bits);
            const uint32_t slice_type = r.ue();
            if (slice_type != 2) {
                return false;   // P or B: not read
            }
            if (p.output_flag_present) {
                r.bit();
            }
            if (s.separate_colour_plane) {
                r.bits(2);
            }
            if (type != 19 && type != 20) {   // not IDR
                r.bits(s.log2_max_poc_lsb);
                if (!r.bit()) {   // short_term_ref_pic_set_sps_flag
                    if (!hevc_skip_st_ref_pic_set(r, s)) {
                        return false;
                    }
                } else if (s.num_short_term_ref_pic_sets > 1) {
                    r.bits(hevc_ceil_log2(s.num_short_term_ref_pic_sets));
                }
                if (s.long_term_ref_pics_present) {
                    uint32_t from_sps = 0;
                    if (s.num_long_term_ref_pics_sps > 0) {
                        from_sps = r.ue();
                    }
                    const uint32_t own_pics = r.ue();
                    if (from_sps > 32 || own_pics > 32) {
                        return false;
                    }
                    for (uint32_t i = 0; i < from_sps + own_pics; ++i) {
                        if (i < from_sps) {
                            if (s.num_long_term_ref_pics_sps > 1) {
                                r.bits(hevc_ceil_log2(s.num_long_term_ref_pics_sps));
                            }
                        } else {
                            r.bits(s.log2_max_poc_lsb);
                            r.bit();
                        }
                        if (r.bit()) {
                            r.ue();
                        }
                    }
                }
                if (s.temporal_mvp) {
                    r.bit();
                }
            }
            bool sao = false;
            if (s.sample_adaptive_offset) {
                sao = r.bit();
                if (s.chroma_format_idc != 0 && !s.separate_colour_plane) {
                    sao = r.bit() || sao;
                }
            }
            r.se();   // slice_qp_delta
            if (p.slice_chroma_qp_offsets_present) {
                r.se(), r.se();
            }
            if (p.slice_act_qp_offsets) {
                r.se(), r.se(), r.se();
            }
            if (p.chroma_qp_offset_list) {
                r.bit();
            }
            bool deblocking_disabled = p.deblocking_disabled;
            if (p.deblocking_override && r.bit()) {
                deblocking_disabled = r.bit();
                if (!deblocking_disabled) {
                    r.se(), r.se();
                }
            }
            if (p.loop_filter_across_slices && (sao || !deblocking_disabled)) {
                r.bit();
            }
        }
        if (r.over() || (!p.tiles && !p.entropy_coding_sync)) {
            return false;
        }
        const uint32_t points = r.ue();
        if (r.over() || points == 0) {
            return false;
        }
        // each subset holds a byte at least: more points than bytes left
        // cannot fit, however the rest of the header reads
        if (points >= size - std::min(size, r.offset())) {
            return true;
        }
        const uint32_t length = r.ue() + 1;
        if (r.over() || length > 32) {
            return false;
        }
        uint64_t sum = 0;
        for (uint32_t i = 0; i < points; ++i) {
            sum += uint64_t(r.bits(length)) + 1;
            if (r.over()) {
                return true;   // the points run past the unit itself
            }
        }
        if (p.slice_header_extension) {
            const uint32_t n = r.ue();
            for (uint32_t i = 0; i < n && !r.over(); ++i) {
                r.bits(8);
            }
        }
        r.bit();   // alignment_bit_equal_to_one
        r.align();
        if (r.over()) {
            return true;   // the header itself ends past the unit
        }
        return sum >= size - r.offset();
    }

    // Where an item's data lies (iloc, ISO/IEC 14496-12 8.11.3): its
    // construction method and its extents, an index into the list of all
    struct HeifLocation {
        uint32_t id = 0;
        unsigned method = 0;
        uint64_t base = 0;
        size_t first = 0;
        size_t count = 0;
    };

    struct HeifExtent {
        uint64_t offset = 0;
        uint64_t length = 0;
    };

    struct HeifAssociation {
        uint32_t item = 0;
        uint32_t property = 0;   // 1-based, into ipco
    };

    // The boxes of a file's meta the guard reads
    struct HeifMeta {
        std::vector<uint32_t> hevc_items;   // infe: hvc1 or hev1
        std::vector<HeifLocation> locations;
        std::vector<HeifExtent> extents;
        std::vector<HeifBox> properties;
        std::vector<HeifAssociation> associations;
        size_t idat = 0;
        size_t idat_end = 0;
        bool has_idat = false;
    };

    inline void heif_read_iinf(const uint8_t* p, const HeifBox& box, HeifMeta& m) {
        if (box.end - box.body < 6) {
            return;
        }
        const size_t at = box.body + 4 + (p[box.body] == 0 ? 2 : 4);
        HeifBoxes entries(p, std::min(at, box.end), box.end);
        for (HeifBox e; entries.next(e);) {
            if (e.type != heif_fourcc("infe") || e.end - e.body < 4) {
                continue;
            }
            const unsigned version = p[e.body];
            if (version < 2) {
                continue;   // no item_type before version 2
            }
            const size_t id_bytes = version == 2 ? 2 : 4;
            if (e.end - e.body < 4 + id_bytes + 2 + 4) {
                continue;
            }
            const uint32_t type = heif_be(p + e.body + 4 + id_bytes + 2, 4);
            if (type == heif_fourcc("hvc1") || type == heif_fourcc("hev1")) {
                m.hevc_items.push_back(heif_be(p + e.body + 4, unsigned(id_bytes)));
            }
        }
    }

    inline void heif_read_iloc(const uint8_t* p, const HeifBox& box, HeifMeta& m) {
        size_t at = box.body;
        const size_t end = box.end;
        auto take = [&](size_t n, uint64_t& v) {
            if (n > end - at) {
                return false;
            }
            v = heif_be64(p + at, unsigned(n));
            at += n;
            return true;
        };
        uint64_t version, flags, sizes, more, items;
        if (!take(1, version) || !take(3, flags) || !take(1, sizes) || !take(1, more) || version > 2) {
            return;
        }
        const size_t offset_size = size_t(sizes >> 4), length_size = size_t(sizes & 15), base_size = size_t(more >> 4);
        const size_t index_size = version >= 1 ? size_t(more & 15) : 0;
        for (size_t n : {offset_size, length_size, base_size, index_size}) {
            if (n != 0 && n != 4 && n != 8) {
                return;
            }
        }
        if (!take(version < 2 ? 2 : 4, items)) {
            return;
        }
        for (uint64_t i = 0; i < items; ++i) {
            HeifLocation l;
            uint64_t id, method = 0, reference, extents;
            if (!take(version < 2 ? 2 : 4, id) || (version >= 1 && !take(2, method)) || !take(2, reference) || !take(base_size, l.base) ||
                !take(2, extents)) {
                return;
            }
            l.id = uint32_t(id);
            l.method = unsigned(method & 15);
            l.first = m.extents.size();
            l.count = size_t(extents);
            for (uint64_t k = 0; k < extents; ++k) {
                uint64_t index;
                HeifExtent x;
                if (!take(index_size, index) || !take(offset_size, x.offset) || !take(length_size, x.length)) {
                    return;
                }
                m.extents.push_back(x);
            }
            m.locations.push_back(l);
        }
    }

    inline void heif_read_ipma(const uint8_t* p, const HeifBox& box, HeifMeta& m) {
        if (box.end - box.body < 8) {
            return;
        }
        const unsigned version = p[box.body];
        const bool wide = p[box.body + 3] & 1;
        const uint32_t entries = heif_be(p + box.body + 4, 4);
        size_t at = box.body + 8;
        for (uint32_t i = 0; i < entries; ++i) {
            const size_t id_bytes = version < 1 ? 2 : 4;
            if (box.end - at < id_bytes + 1) {
                return;
            }
            const uint32_t item = heif_be(p + at, unsigned(id_bytes));
            const unsigned count = p[at + id_bytes];
            at += id_bytes + 1;
            for (unsigned k = 0; k < count; ++k) {
                const size_t n = wide ? 2 : 1;
                if (box.end - at < n) {
                    return;
                }
                const uint32_t index = heif_be(p + at, unsigned(n)) & (wide ? 0x7FFFu : 0x7Fu);
                at += n;
                m.associations.push_back({item, index});
            }
        }
    }

    // An item's data as the guard reads it: in place when it is one
    // extent, else its extents one after another, never more bytes than
    // the file has; false when it lies elsewhere (construction method 2)
    // or outside the file
    inline bool heif_item_data(const uint8_t* p, size_t size, const HeifMeta& m, const HeifLocation& l, std::vector<uint8_t>& joined,
                               const uint8_t*& data, size_t& length, size_t& at) {
        uint64_t origin = 0, limit = size;
        if (l.method == 1) {
            if (!m.has_idat) {
                return false;
            }
            origin = m.idat;
            limit = m.idat_end;
        } else if (l.method != 0) {
            return false;
        }
        joined.clear();
        for (size_t k = 0; k < l.count; ++k) {
            const HeifExtent& x = m.extents[l.first + k];
            if (l.base >= limit || x.offset >= limit) {
                return false;
            }
            const uint64_t from = origin + l.base + x.offset;
            if (from >= limit) {
                return false;
            }
            const uint64_t n = x.length == 0 ? limit - from : std::min<uint64_t>(x.length, limit - from);
            if (l.count == 1) {
                data = p + from;
                length = size_t(n);
                at = size_t(from);
                return true;
            }
            const size_t take = size_t(std::min<uint64_t>(n, size - joined.size()));
            const size_t old = joined.size();
            joined.resize(old + take);
            copy_bytes(joined.data() + old, p + from, take);
            if (take < n) {
                break;
            }
        }
        data = joined.data();
        length = joined.size();
        at = 0;
        return length != 0;
    }

    // The guard: errc::corrupt for a file with an HEVC slice whose entry
    // points lie past its data, at the offset of its NAL unit (0 when its
    // item lies in pieces); nullopt for everything else
    inline optional<error> heif_guard(const uint8_t* p, size_t size) noexcept {
        HeifBoxes top(p, 0, size);
        HeifBox meta;
        bool found = false;
        for (HeifBox b; top.next(b);) {
            if (b.type == heif_fourcc("meta")) {
                meta = b;
                found = true;
                break;
            }
        }
        if (!found || meta.end - meta.body < 4) {
            return nullopt;
        }
        HeifMeta m;
        HeifBoxes children(p, meta.body + 4, meta.end);
        for (HeifBox b; children.next(b);) {
            if (b.type == heif_fourcc("iinf")) {
                heif_read_iinf(p, b, m);
            } else if (b.type == heif_fourcc("iloc")) {
                heif_read_iloc(p, b, m);
            } else if (b.type == heif_fourcc("idat")) {
                m.idat = b.body;
                m.idat_end = b.end;
                m.has_idat = true;
            } else if (b.type == heif_fourcc("iprp")) {
                HeifBoxes inner(p, b.body, b.end);
                for (HeifBox c; inner.next(c);) {
                    if (c.type == heif_fourcc("ipco")) {
                        HeifBoxes properties(p, c.body, c.end);
                        for (HeifBox q; properties.next(q);) {
                            m.properties.push_back(q);
                        }
                    } else if (c.type == heif_fourcc("ipma")) {
                        heif_read_ipma(p, c, m);
                    }
                }
            }
        }
        // the HEVC items: hvc1 by their entry, or hvcC among their
        // properties. The work is bounded by the file: items whose data or
        // configurations, read once per item, come to more than four times
        // the file (they overlap: no encoder writes that) are refused
        std::sort(m.hevc_items.begin(), m.hevc_items.end());
        std::stable_sort(m.associations.begin(), m.associations.end(),
                         [](const HeifAssociation& a, const HeifAssociation& b) { return a.item < b.item; });
        uint64_t budget = uint64_t(size) * 4 + (uint64_t(1) << 20);
        std::vector<uint8_t> joined;
        for (const HeifLocation& l : m.locations) {
            const HeifBox* config = nullptr;
            auto [first, last] = std::equal_range(m.associations.begin(), m.associations.end(), HeifAssociation{l.id, 0},
                                                  [](const HeifAssociation& a, const HeifAssociation& b) { return a.item < b.item; });
            for (auto a = first; a != last; ++a) {
                if (a->property >= 1 && a->property <= m.properties.size() && m.properties[a->property - 1].type == heif_fourcc("hvcC")) {
                    config = &m.properties[a->property - 1];
                    break;
                }
            }
            if (!config && !std::binary_search(m.hevc_items.begin(), m.hevc_items.end(), l.id)) {
                continue;
            }
            const uint8_t* data = nullptr;
            size_t length = 0, origin = 0;
            if (!heif_item_data(p, size, m, l, joined, data, length, origin)) {
                continue;
            }
            const uint64_t cost = length + (config ? config->end - config->body : 0);
            if (cost > budget) {
                return error(errc::unsupported, 0, "heif: HEVC items overlapping past four times the file");
            }
            budget -= cost;
            HevcSets sets;
            size_t length_size = 4;
            if (config) {
                // HEVCDecoderConfigurationRecord (ISO/IEC 14496-15 8.3.3.1)
                const uint8_t* c = p + config->body;
                const size_t n = config->end - config->body;
                if (n < 23) {
                    continue;
                }
                length_size = (c[21] & 3) + 1u;
                size_t at = 23;
                for (unsigned a = 0; a < c[22] && n - at >= 3; ++a) {
                    const unsigned nalus = heif_be(c + at + 1, 2);
                    at += 3;
                    for (unsigned k = 0; k < nalus && n - at >= 2; ++k) {
                        const size_t len = heif_be(c + at, 2);
                        if (n - at - 2 < len) {
                            at = n;
                            break;
                        }
                        sets.add(c + at + 2, len);
                        at += 2 + len;
                    }
                }
            }
            // the NAL units, each after its length (ISO/IEC 14496-15 4.3.2)
            for (size_t at = 0; length - at >= length_size;) {
                const size_t len = heif_be(data + at, unsigned(length_size));
                at += length_size;
                if (len > length - at) {
                    break;   // a unit cut short: ImageIO's to judge
                }
                const uint8_t* nal = data + at;
                if (len >= 3 && ((nal[0] & 1) << 5 | nal[1] >> 3) == 0) {
                    const unsigned type = nal[0] >> 1 & 63;
                    if (type == 33 || type == 34) {
                        sets.add(nal, len);
                    } else if ((type <= 9 || (type >= 16 && type <= 21)) && hevc_entry_points_past_end(nal, len, sets)) {
                        return error(errc::corrupt, joined.empty() ? uint64_t(origin + at) : 0,
                                     "heif: an HEVC slice whose entry points lie past its data");
                    }
                }
                at += len;
            }
        }
        return nullopt;
    }
}
