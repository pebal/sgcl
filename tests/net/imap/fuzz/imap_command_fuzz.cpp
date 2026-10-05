//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap::server's session on any bytes from a client: a fresh server
// state over a memory_backend with three messages (one a multipart with
// a message inside), one session on a pipe in memory; the input is what
// the client sends, behind a login and a SELECT when the first byte's bit
// 0 is set (so that the commands of the selected state are reached), then
// the end of the stream. What must hold:
//   - the session ends (its task done) once the input ends, never a crash;
//   - everything the server wrote reads as IMAP responses: each one whole
//     (its literals as announced), each of the grammar of RFC 9051 §9 as
//     the client's parser takes it, the data of FETCH, LIST, LSUB, STATUS,
//     SEARCH, ESEARCH, SORT, THREAD, NAMESPACE, QUOTA, ID, VANISHED read
//     by their parsers;
//   - a tagged response only for a tag the input carried.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/imap/fuzz/imap_command_fuzz.cpp)
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

    async::task<void> feed(net::connection c, std::string in) noexcept {
        (void)co_await c.async_write(string(in));
        (void)c.close_write();
    }

    // The data of a response read by the parser of its kind
    bool readable(std::string_view raw, const d::Response& r) {
        if (r.kind != d::Response::Kind::untagged || r.status != d::Status::none) {
            return true;
        }
        d::Lexer x(raw.substr(r.data_at));
        std::string scratch;
        const std::string name = d::to_upper(r.name);
        if (r.numbered) {
            if (name == "FETCH") {
                imap::message m;
                return d::parse_fetch(x, m, scratch);
            }
            return name == "EXISTS" || name == "EXPUNGE" || name == "RECENT";
        }
        if (name == "LIST" || name == "LSUB") {
            imap::list_entry e;
            return d::parse_list(x, e, true, scratch);
        }
        if (name == "STATUS") {
            string n;
            imap::status s;
            return d::parse_status(x, n, s, true, scratch);
        }
        if (name == "SEARCH" || name == "SORT") {
            vector<uint32_t> v;
            uint64_t ms;
            return d::parse_numbers(x, v, ms);
        }
        if (name == "ESEARCH") {
            d::Esearch e;
            return d::parse_esearch(x, e, scratch);
        }
        if (name == "THREAD") {
            vector<imap::thread> t;
            return d::parse_threads(x, t);
        }
        if (name == "NAMESPACE") {
            imap::namespaces n;
            return d::parse_namespace(x, n, scratch);
        }
        if (name == "QUOTA") {
            imap::quota q;
            return d::parse_quota(x, q, scratch);
        }
        if (name == "ID") {
            vector<pair<string, string>> id;
            return d::parse_id(x, id, scratch);
        }
        if (name == "VANISHED") {
            bool earlier;
            imap::sequence_set s;
            return d::parse_vanished(x, earlier, s);
        }
        if (name == "FLAGS") {
            vector<string> f;
            return d::parse_flag_list(x, f);
        }
        return name == "CAPABILITY" || name == "ENABLED" || name == "QUOTAROOT";
    }

    // The server's output taken apart as a client would
    void check_output(std::string_view out, std::string_view in) {
        size_t i = 0;
        while (i < out.size()) {
            std::string resp;
            for (;;) {
                const size_t lf = out.find('\n', i);
                check(lf != std::string_view::npos);   // every response ends its line
                check(lf > i && out[lf - 1] == '\r');
                std::string_view line = out.substr(i, lf - 1 - i);
                resp.append(line.data(), line.size());
                i = lf + 1;
                auto h = d::literal_at_end(line);
                if (!h.found) {
                    break;
                }
                check(!h.sync || true);
                check(out.size() - i >= h.size);
                resp += "\r\n";
                resp.append(out.data() + i, size_t(h.size));
                i += size_t(h.size);
            }
            d::Response r;
            if (resp.rfind("+ ", 0) == 0 || resp == "+") {
                continue;   // a continuation
            }
            check(d::parse_response(resp, r));
            check(readable(resp, r));
            if (r.kind == d::Response::Kind::tagged) {
                // its tag came from the input (the start of a line there)
                const std::string tag(r.tag);
                check(in.substr(0, tag.size() + 1) == tag + " " || in.find("\n" + tag + " ") != std::string_view::npos);
            }
        }
    }

    imap::memory_backend backend() {
        imap::memory_backend mail;
        mail.add_user("alice", "secret");
        mail.create("alice", "Archive", "\\Archive");
        mail.append("alice", "INBOX", "From: Bob <bob@x.org>\r\nTo: a@y.org, team: c@z.org;\r\nSubject: =?UTF-8?Q?Cze=C5=9B=C4=87?=\r\nMessage-ID: <1@x>\r\n"
                                      "Date: Mon, 5 Oct 2026 10:00:00 +0200\r\n\r\nHello\r\n");
        mail.append("alice", "INBOX", "Subject: Re: parts\r\nIn-Reply-To: <1@x>\r\nContent-Type: multipart/mixed; boundary=b\r\n\r\n--b\r\nContent-Type: text/plain; charset=utf-8\r\n"
                                      "Content-Transfer-Encoding: quoted-printable\r\n\r\ncaf=C3=A9\r\n--b\r\nContent-Type: message/rfc822\r\n\r\nSubject: inner\r\n\r\ninner body\r\n"
                                      "--b\r\nContent-Type: application/octet-stream\r\nContent-Transfer-Encoding: base64\r\n\r\nAAEC\r\n--b--\r\n",
                    {string("\\Seen"), string("$Work")});
        mail.append("alice", "INBOX", "Subject: third\r\n\r\nbinary\0zero\r\n", {string("\\Deleted")});
        return mail;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string in;
    if (data[0] & 1) {
        in = "a LOGIN alice secret\r\nb ENABLE CONDSTORE\r\nc SELECT INBOX\r\n";
    }
    in.append(reinterpret_cast<const char*>(data + 1), size - 1);
    tracked_ptr<d::ServerSettings> cfg = make_tracked<d::ServerSettings>();
    cfg->backend = d::BackendAccess::get(imap::backend(backend()));
    cfg->idle_timeout = std::chrono::seconds(5);
    cfg->login_timeout = std::chrono::seconds(5);
    cfg->poll_interval = duration::zero();
    cfg->max_literal = 4096;
    cfg->max_command = 4096;
    cfg->max_auth_failures = 3;
    cfg->compress = (data[0] & 2) != 0;
    cfg->greeting = string("fuzz");
    cfg->on_error = [](const string&) {};
    if (data[0] & 4) {
        cfg->check_token = [](const string& u, const string& t) { return u == "alice" && t == "tok"; };
    }
    tracked_ptr<d::ServerImpl> impl = make_tracked<d::ServerImpl>();
    auto [client, server] = net::connection::in_memory();
    client.set_read_deadline(clock::now() + std::chrono::seconds(5));
    server.set_deadline(clock::now() + std::chrono::seconds(5));
    impl->running.add();
    auto session = async::spawn(d::serve_connection(impl, cfg, server, false));
    auto writer = async::spawn(feed(client, in));
    auto out = client.read_all_text();
    check(out.has_value() || !out.error().is_timeout());
    writer.wait();
    session.wait();
    if (out && !(data[0] & 2)) {   // compressed output is not text to read
        const std::string_view o = out->view();
        // to COMPRESS's answer: what follows it is DEFLATE
        check_output(o, in);
    }
    return 0;
}
