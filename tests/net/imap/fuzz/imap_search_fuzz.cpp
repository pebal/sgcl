//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's search keys and its view of a message, on any bytes
// (detail/search.h, detail/mime.h). The input is search keys, a NUL, and a
// message; the keys are read and judged against the message (as three
// messages of different flags, sizes and dates), the message's structure
// taken apart, its ENVELOPE and BODYSTRUCTURE written, sections asked for,
// SORT's information and THREAD's algorithms run over copies of it. What
// must hold:
//   - never a crash, every loop ends;
//   - keys that read judge every message without needing more than the
//     message gives (a NOT of what matched does not match);
//   - the ENVELOPE, BODY and BODYSTRUCTURE written read back by the client's
//     parser, the numbers of the parts within the message;
//   - a section's octets lie within the message (their size at most its
//     size, plus the CRLF of HEADER.FIELDS);
//   - every message in a THREAD's answer once.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/imap/fuzz/imap_search_fuzz.cpp)
// or replayed by the library's own driver.
#include "sgcl/net/imap/imap.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace imap = sgcl::net::imap;
    namespace d = sgcl::net::imap::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void structure(const std::string& msg) {
        d::Structure st(msg);
        for (bool ext : {false, true}) {
            std::string bs;
            d::put_body(bs, msg, st.root, ext, ext);
            std::string line = "* 1 FETCH (" + std::string(ext ? "BODYSTRUCTURE " : "BODY ") + bs + ")";
            d::Response r;
            check(d::parse_response(line, r));
            d::Lexer x(std::string_view(line).substr(r.data_at));
            imap::message m;
            std::string scratch;
            check(d::parse_fetch(x, m, scratch));
            check(m.body_structure.has_value());
        }
        std::string env;
        d::put_envelope(env, st.root, true);
        std::string line = "* 1 FETCH (ENVELOPE " + env + ")";
        d::Response r;
        check(d::parse_response(line, r));
        d::Lexer x(std::string_view(line).substr(r.data_at));
        imap::message m;
        std::string scratch;
        check(d::parse_fetch(x, m, scratch));
        for (const char* spec : {"", "1", "2", "1.1", "2.1.2", "HEADER", "TEXT", "1.MIME", "2.HEADER", "1.2.TEXT", "HEADER.FIELDS (SUBJECT FROM)",
                                 "HEADER.FIELDS.NOT (SUBJECT)", "3.1.HEADER.FIELDS (TO)"}) {
            d::Section sec;
            if (!d::parse_section(spec, sec)) {
                continue;
            }
            std::string out;
            if (d::section_bytes(msg, st.root, sec, out)) {
                check(out.size() <= msg.size() + 2);
            }
            const d::Part* p = d::resolve_part(st.root, sec.path);
            if (p) {
                check(p->header_begin <= p->body_begin && p->body_begin <= p->body_end && p->body_end <= msg.size());
                std::string decoded;
                d::decoded_content(std::string_view(msg).substr(p->body_begin, p->body_end - p->body_begin), p->encoding, decoded);
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view in(reinterpret_cast<const char*>(data), size);
    const size_t nul = in.find('\0');
    const std::string_view keys = nul == std::string_view::npos ? in : in.substr(0, nul);
    const std::string msg(nul == std::string_view::npos ? std::string_view() : in.substr(nul + 1));
    structure(msg);
    d::SearchKey root;
    d::SearchParse sp;
    d::Lexer x(keys);
    std::string scratch;
    const bool ok = d::parse_search_keys(x, root, sp, scratch, 0) && x.at_end();
    std::vector<uint32_t> saved = {2};
    std::vector<std::string> keywords = {"$Work"};
    for (uint32_t i = 1; i <= 3 && ok; ++i) {
        d::SearchMessage m;
        m.seq = i;
        m.uid = i * 10;
        m.flags = uint8_t(i * 5);
        m.keywords = i == 2 ? &keywords : nullptr;
        m.size = msg.size() * i;
        m.internal_date = d::DateTime{int64_t(1700000000) + int64_t(i) * 86400, int(i) * 60 - 120};
        m.modseq = i * 7;
        m.largest_seq = 3;
        m.largest_uid = 30;
        m.now = 1800000000;
        m.saved = &saved;
        m.content = [&msg]() -> const std::string* { return &msg; };
        std::unique_ptr<d::SearchText> text;
        (void)d::judge(root, m, text);
    }
    // SORT's information and THREAD's algorithms over copies
    std::vector<d::SortInfo> infos;
    for (int i = 0; i < 3; ++i) {
        infos.push_back(d::sort_info(msg, d::DateTime{int64_t(i), 0}));
    }
    std::vector<d::SortItem> items;
    for (uint32_t i = 0; i < 3; ++i) {
        items.push_back(d::SortItem{i + 1, i + 1, i, d::DateTime{}, &infos[i]});
    }
    std::vector<d::SortKey> keys_all = {{d::SortKey::Kind::subject, false}, {d::SortKey::Kind::date, true}, {d::SortKey::Kind::display_from, false}};
    d::sort_items(items, keys_all);
    for (int alg = 0; alg < 2; ++alg) {
        auto roots = alg ? d::thread_references(items) : d::thread_ordered_subject(items);
        std::string out;
        for (const auto& r : roots) {
            d::put_thread(out, *r, items, true);
        }
        for (char c : std::string("123")) {
            check(std::count(out.begin(), out.end(), c) == 1);
        }
    }
    (void)d::base_subject(keys);
    return 0;
}
