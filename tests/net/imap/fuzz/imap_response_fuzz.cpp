//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap::client on any bytes from a server. The first byte picks the
// mode: bit 0 clear, the rest is one response taken apart by the client's
// parser (its kind, status and code, then its data by the parser of its
// name, and the client's state updated as an untagged response updates it);
// bit 0 set, the rest is everything a server sends to a client that
// connects (greeting, capabilities, the answers to LOGIN, ENABLE, SELECT,
// UID FETCH of every item, UID SEARCH, LIST, STATUS and IDLE) over a pipe in
// memory, the client's own commands read and dropped. What must hold:
//   - never a crash, never a wait past the input's end: every call returns,
//     a value or an error;
//   - an error is of the documented kinds (the imap category, io's closed
//     and end of stream, the system's for a pipe closed);
//   - a fetch's messages each have a UID of the set asked.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/imap/fuzz/imap_response_fuzz.cpp)
// or replayed by the library's own driver.
#include "sgcl/net/imap/imap.h"
#include "sgcl/net/net.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace imap = sgcl::net::imap;
    namespace d = sgcl::net::imap::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool documented(const io::error& e) {
        const auto& cat = e.code().category();
        return cat == imap::category() || e.code() == io::errc::closed || e.code() == io::errc::unexpected_eof || cat == std::system_category() ||
               cat == std::generic_category() || e.code() == net::errc::invalid_url;
    }

    void parse_one(std::string_view s) {
        d::Response r;
        if (!d::parse_response(s, r)) {
            return;
        }
        if (r.kind == d::Response::Kind::untagged && r.status == d::Status::none) {
            d::Lexer x(s.substr(r.data_at));
            x.set_lenient(true);
            std::string scratch;
            const std::string name = d::to_upper(r.name);
            imap::message m;
            imap::list_entry e;
            string n;
            imap::status st;
            vector<uint32_t> v;
            uint64_t ms;
            d::Esearch es;
            vector<imap::thread> t;
            imap::namespaces ns;
            imap::quota q;
            vector<pair<string, string>> id;
            bool earlier;
            imap::sequence_set set;
            if (name == "FETCH") {
                if (d::parse_fetch(x, m, scratch) && m.body_structure) {
                    check(m.body_structure->part.size() < s.size() + 2);
                }
            } else if (name == "LIST" || name == "LSUB") {
                d::parse_list(x, e, s.size() & 1, scratch);
            } else if (name == "STATUS") {
                d::parse_status(x, n, st, true, scratch);
            } else if (name == "SEARCH" || name == "SORT") {
                d::parse_numbers(x, v, ms);
            } else if (name == "ESEARCH") {
                d::parse_esearch(x, es, scratch);
            } else if (name == "THREAD") {
                d::parse_threads(x, t);
            } else if (name == "NAMESPACE") {
                d::parse_namespace(x, ns, scratch);
            } else if (name == "QUOTA") {
                d::parse_quota(x, q, scratch);
            } else if (name == "ID") {
                d::parse_id(x, id, scratch);
            } else if (name == "VANISHED") {
                d::parse_vanished(x, earlier, set);
            }
        } else if (!r.code.empty()) {
            imap::copy_result c;
            d::parse_copyuid(r.code_data, c);
            check(c.source.size() == c.destination.size());
            uint32_t validity;
            vector<uint32_t> uids;
            d::parse_appenduid(r.code_data, validity, uids);
        }
        auto impl = make_tracked<d::ClientImpl>();
        impl->has_mailbox = true;
        d::take_untagged(*impl, s, r);
    }

    async::task<void> server_side(net::connection c, std::string out) noexcept {
        (void)co_await c.async_write(string(out));
        (void)c.close_write();
        // the client's commands drained to its end
        char buf[4096];
        for (;;) {
            auto r = co_await c.async_read(slice<byte>(reinterpret_cast<byte*>(buf), sizeof(buf)));
            if (!r || *r == 0) {
                break;
            }
        }
    }

    void session(std::string_view in) {
        auto [client_end, server_end] = net::connection::in_memory();
        client_end.set_deadline(clock::now() + std::chrono::seconds(5));
        auto srv = async::spawn(server_side(server_end, std::string(in)));
        imap::client::options o;
        o.security = imap::security::none;
        o.user = "alice";
        o.password = "secret";
        o.timeout = std::chrono::seconds(5);
        o.max_literal_bytes = 1 << 16;
        auto c = imap::client::connect(client_end, o);
        if (c) {
            auto s = c->select("INBOX");
            check(s || documented(s.error()));
            imap::fetch_options f;
            f.envelope = f.body_structure = f.size = f.internal_date = f.modseq = true;
            f.sections = {string(""), string("1.2")};
            auto m = c->fetch(imap::sequence_set(1, 5), f);
            check(m || documented(m.error()));
            if (m) {
                for (const auto& x : *m) {
                    check(x.uid >= 1 && x.uid <= 5);
                }
            }
            auto se = c->search(imap::criteria::unseen());
            check(se || documented(se.error()));
            auto l = c->list();
            check(l || documented(l.error()));
            auto st = c->status("INBOX");
            check(st || documented(st.error()));
            auto i = c->idle(std::chrono::milliseconds(1));
            check(i || documented(i.error()));
            (void)c->close();
        } else {
            check(documented(c.error()));
        }
        (void)client_end.close();
        srv.wait();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 1);
    if (data[0] & 1) {
        session(in);
    } else {
        parse_one(in);
    }
    return 0;
}
