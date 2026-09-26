//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http: the parser, pure, by vectors. One test for each rule of the
// parser's list (sgcl/net/http/detail/parser.h, RFC 9112 and RFC 9110),
// named by its section, the expected statuses written from the RFCs; the
// known request smuggling payloads (CL.TE, TE.CL, TE.TE in the spellings
// that have fooled servers, LF in chunked framing, two lengths,
// xchunked, obs-fold); chunked cut at every byte and in pieces of 1, 2, 3
// and 7; the response side; and a deterministic mutator over valid
// requests, whose seeds are in the case names, run under ASan in the
// sanitizer build.
#include "tests/types.h"
#include "sgcl/net/http/detail/parser.h"

#include <random>
#include <string>
#include <vector>

using namespace sgcl;
using namespace sgcl::net::http;
using namespace sgcl::net::http::detail;

namespace {
    struct Checked {
        int status = -1;             // 0: accepted; -1: no whole head
        BodyFraming framing;
        headers fields;
        RequestLine line;
    };

    Checked check(const std::string& bytes) {
        Checked c;
        size_t skip = leading_empty_lines(bytes.data(), bytes.size());
        size_t end = find_head_end(bytes.data() + skip, bytes.size() - skip);
        if (!end) {
            return c;
        }
        string head(std::string_view(bytes).substr(skip, end));
        c.status = check_request_head(head, c.line, c.fields, c.framing);
        return c;
    }

    int status_of(const std::string& bytes) {
        return check(bytes).status;
    }

    std::string rq(const std::string& fields, const std::string& line = "GET / HTTP/1.1") {
        return line + "\r\n" + fields + "\r\n";
    }

    // The data of a chunked body fed to the decoder in pieces of `piece`
    // bytes, taken `room` at a time: the data, "!" and the status on an
    // error, "?" when it never ended
    std::string dechunk(const std::string& encoded, size_t piece, size_t room = 5, headers* trailers = nullptr) {
        ChunkedDecoder d;
        std::string buffer, out;
        size_t fed = 0;
        for (;;) {
            if (fed < encoded.size()) {
                size_t n = std::min(piece, encoded.size() - fed);
                buffer.append(encoded, fed, n);
                fed += n;
            }
            for (;;) {
                auto s = d.step(buffer, room);
                if (s.error) {
                    return "!" + std::to_string(s.error);
                }
                out.append(buffer, s.data_at, s.data_size);
                buffer.erase(0, s.consumed);
                if (s.done) {
                    if (trailers) {
                        *trailers = d.trailers();
                    }
                    return out + (buffer.empty() ? "" : "|rest:" + buffer);
                }
                if (s.consumed == 0) {
                    break;
                }
            }
            if (fed == encoded.size() && buffer.empty()) {
                return "?" + out;
            }
            if (fed == encoded.size()) {
                return "?" + out;
            }
        }
    }
}

// RFC 9112 §3: the request line
TEST(HttpParser_Tests, RequestLine_3) {
    EXPECT_EQ(status_of(rq("Host: x\r\n")), 0);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET  / HTTP/1.1")), 400);          // two spaces
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/1.1 ")), 400);          // a space at the end
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET\t/ HTTP/1.1")), 400);          // a tab
    EXPECT_EQ(status_of(rq("Host: x\r\n", " GET / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "G(T / HTTP/1.1")), 400);           // not a token
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET /a\x01 HTTP/1.1")), 400);      // a control in the target
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET /\xC3\xA9 HTTP/1.1")), 400);   // not ASCII
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET /a\x7F HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET /")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/1.1\r")), 400);         // a bare CR
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET /a\rb HTTP/1.1")), 400);
    auto c = check(rq("Host: x\r\n", "DELETE /a?b=c HTTP/1.1"));
    EXPECT_EQ(c.status, 0);
    EXPECT_EQ(c.line.method_size, 6u);
    EXPECT_EQ(c.line.target_size, 6u);
}

// RFC 9112 §2.3: the version, exactly HTTP/1.0 or HTTP/1.1
TEST(HttpParser_Tests, Version_2_3) {
    EXPECT_EQ(check(rq("", "GET / HTTP/1.0")).line.minor, 0);
    EXPECT_EQ(status_of(rq("", "GET / HTTP/1.0")), 0);                        // no Host needed in 1.0
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/1.2")), 505);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/2.0")), 505);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/0.9")), 505);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / http/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/1.1x")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/11.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET / HTTP/1.a")), 400);
}

// RFC 9112 §3.2: the four forms of the target
TEST(HttpParser_Tests, TargetForms_3_2) {
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET http://x/a HTTP/1.1")), 0);    // absolute
    EXPECT_EQ(status_of(rq("Host: x\r\n", "OPTIONS * HTTP/1.1")), 0);         // asterisk
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET * HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x:80\r\n", "CONNECT x:80 HTTP/1.1")), 0);   // authority
    EXPECT_EQ(status_of(rq("Host: x\r\n", "CONNECT / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET a/b HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET :x HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n", "GET 1http://x/ HTTP/1.1")), 400);
}

// RFC 9112 §2.2: a bare CR anywhere is 400; a bare LF ends a line
TEST(HttpParser_Tests, LineEnds_2_2) {
    EXPECT_EQ(status_of("GET / HTTP/1.1\nHost: x\n\n"), 0);
    EXPECT_EQ(status_of("GET / HTTP/1.1\r\nHost: x\n\r\n"), 0);
    EXPECT_EQ(status_of("GET / HTTP/1.1\r\nHost: x\r\nA: b\rc\r\n\r\n"), 400);
    EXPECT_EQ(status_of("GET / HTTP/1.1\r\nHost: x\r\nA\r: b\r\n\r\n"), 400);
    EXPECT_EQ(status_of("GET / HTTP/1.1\r\nHost: x\r\n\r\r\n\r\n"), 400);
    // empty lines before the request line are skipped (§2.2 SHOULD)
    EXPECT_EQ(status_of("\r\n\r\nGET / HTTP/1.1\r\nHost: x\r\n\r\n"), 0);
}

// RFC 9112 §5.1: no whitespace between a name and its colon
TEST(HttpParser_Tests, NameColon_5_1) {
    EXPECT_EQ(status_of(rq("Host : x\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nA\t: b\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\n: b\r\n")), 400);                     // no name
    EXPECT_EQ(status_of(rq("Host: x\r\nA(b: c\r\n")), 400);                  // not a token
    EXPECT_EQ(status_of(rq("Host: x\r\nnocolon\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host:x\r\nA:b\r\n")), 0);                        // no OWS at all
}

// RFC 9112 §2.2 and §5.2: whitespace before the first field, obs-fold
TEST(HttpParser_Tests, Folding_5_2) {
    EXPECT_EQ(status_of(rq(" Host: x\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nA: b\r\n c\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nA: b\r\n\tc\r\n")), 400);
}

// RFC 9110 §5.5: a value has no NUL, CR, LF, and no control but HTAB; obs-text passes
TEST(HttpParser_Tests, FieldValues_5_5) {
    EXPECT_EQ(status_of(rq(std::string("Host: x\r\nA: b\0c\r\n", 17))), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nA: b\x01" "c\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nA: b\x7F" "c\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nA: b\tc\r\n")), 0);
    auto c = check(rq("Host: x\r\nA: \t caf\xC3\xA9 \t\r\n"));
    EXPECT_EQ(c.status, 0);
    EXPECT_EQ(c.fields.get("a"), "caf\xC3\xA9");                            // OWS off both ends
    EXPECT_EQ(c.fields.get("A"), "caf\xC3\xA9");
    EXPECT_EQ(c.fields.get("host"), "x");
}

// RFC 9112 §6.1 and §6.3: Transfer-Encoding
TEST(HttpParser_Tests, TransferEncoding_6_1) {
    auto c = check(rq("Host: x\r\nTransfer-Encoding: chunked\r\n", "POST / HTTP/1.1"));
    EXPECT_EQ(c.status, 0);
    EXPECT_EQ(c.framing.kind, Framing::chunked);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: Chunked\r\n", "POST / HTTP/1.1")), 0);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding:\t chunked \t\r\n", "POST / HTTP/1.1")), 0);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: gzip\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: gzip, chunked\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: identity\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked, chunked\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: \r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Transfer-Encoding: chunked\r\n", "POST / HTTP/1.0")), 400);   // not in 1.0
}

// RFC 9112 §6.2 and RFC 9110 §8.6: Content-Length
TEST(HttpParser_Tests, ContentLength_6_2) {
    auto c = check(rq("Host: x\r\nContent-Length: 5\r\n", "POST / HTTP/1.1"));
    EXPECT_EQ(c.status, 0);
    EXPECT_EQ(c.framing.kind, Framing::length);
    EXPECT_EQ(c.framing.length, 5u);
    EXPECT_EQ(check(rq("Host: x\r\nContent-Length: 0\r\n", "POST / HTTP/1.1")).framing.kind, Framing::none);
    EXPECT_EQ(check(rq("Host: x\r\n", "POST / HTTP/1.1")).framing.kind, Framing::none);   // neither: 0
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 5, 5\r\n", "POST / HTTP/1.1")), 0);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 5\r\nContent-Length: 5\r\n", "POST / HTTP/1.1")), 0);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 5, 6\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 5\r\nContent-Length: 6\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: +5\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: -1\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 0x5\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 5 5\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: \r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 5,\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 9223372036854775807\r\n", "POST / HTTP/1.1")), 0);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 9223372036854775808\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 99999999999999999999\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 18446744073709551621\r\n", "POST / HTTP/1.1")), 400);   // wraps to 5 in 64 bits
}

// RFC 9112 §6.3 item 3: Transfer-Encoding and Content-Length together
TEST(HttpParser_Tests, BothFramings_6_3) {
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked\r\nContent-Length: 3\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked\r\nContent-Length: x\r\n", "POST / HTTP/1.1")), 400);
}

// RFC 9112 §3.2: Host
TEST(HttpParser_Tests, Host_3_2) {
    EXPECT_EQ(status_of(rq("")), 400);                                        // 1.1 without one
    EXPECT_EQ(status_of(rq("Host: a\r\nHost: a\r\n")), 400);                 // two
    EXPECT_EQ(status_of(rq("Host: a b\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: a/b\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: a@b\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: a:b\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: a:80:80\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: [::1\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: [::1]x\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: [zz::1]\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: a%zz\r\n")), 400);
    EXPECT_EQ(status_of(rq("Host: \r\n")), 0);                                // empty is an authority
    EXPECT_EQ(status_of(rq("Host: example.com:8080\r\n")), 0);
    EXPECT_EQ(status_of(rq("Host: [::1]:8080\r\n")), 0);
    EXPECT_EQ(status_of(rq("Host: 10.0.0.1\r\n")), 0);
    EXPECT_EQ(status_of(rq("Host: a%41b\r\n")), 0);
    EXPECT_EQ(status_of(rq("Host: a b\r\n", "GET / HTTP/1.0")), 400);         // checked in 1.0 too
}

// The payloads of request smuggling, each refused
TEST(HttpParser_Tests, Smuggling) {
    // CL.TE and TE.CL: a front end reading one, a back end the other
    EXPECT_EQ(status_of("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 13\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\nSMUGGLED"), 400);
    EXPECT_EQ(status_of("POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n8\r\nSMUGGLED\r\n0\r\n\r\n"), 400);
    // TE.TE: a Transfer-Encoding one side does not see
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: x\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: xchunked\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked-false\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding : chunked\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding\r\n : chunked\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nX: y\r\n Transfer-Encoding: chunked\r\n", "POST / HTTP/1.1")), 400);   // folded into X
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: \"chunked\"\r\n", "POST / HTTP/1.1")), 501);
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer-Encoding: chunked\x0B\r\n", "POST / HTTP/1.1")), 400);   // a vertical tab
    EXPECT_EQ(status_of(rq("Host: x\r\nTransfer_Encoding: chunked\r\nContent-Length: 3\r\n", "POST / HTTP/1.1")), 0);   // another field: CL frames
    EXPECT_EQ(check(rq("Host: x\r\nTransfer_Encoding: chunked\r\nContent-Length: 3\r\n", "POST / HTTP/1.1")).framing.kind, Framing::length);
    // two lengths
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 0\r\nContent-Length: 44\r\n", "POST / HTTP/1.1")), 400);
    EXPECT_EQ(status_of(rq("Host: x\r\nContent-Length: 0, 44\r\n", "POST / HTTP/1.1")), 400);
    // a CR alone as a line end, which some read as one and some do not
    EXPECT_EQ(status_of("POST / HTTP/1.1\r\nHost: x\rContent-Length: 44\r\n\r\n"), 400);
    // LF in the chunked framing: strictly CRLF there
    EXPECT_EQ(dechunk("3\nabc\r\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("3\r\nabc\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("3\r\nabc\r\n0\n\n", 1000), "!400");
    EXPECT_EQ(dechunk("3\r\nabc\r\n0\r\n\n", 1000), "!400");
    EXPECT_EQ(dechunk("3\r\nabcX\r\n0\r\n\r\n", 1000), "!400");            // the data longer than its size
    // a size that overflows, or with a sign, a prefix or a space
    EXPECT_EQ(dechunk("10000000000000000\r\nx\r\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("8000000000000000\r\nx", 1000), "!400");
    EXPECT_EQ(dechunk("-3\r\nabc\r\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("0x3\r\nabc\r\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("3 \r\nabc\r\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk(" 3\r\nabc\r\n0\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("\r\nabc\r\n0\r\n\r\n", 1000), "!400");
    // a framing field in the trailers
    EXPECT_EQ(dechunk("3\r\nabc\r\n0\r\nContent-Length: 5\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("3\r\nabc\r\n0\r\nTransfer-Encoding: chunked\r\n\r\n", 1000), "!400");
    EXPECT_EQ(dechunk("3\r\nabc\r\n0\r\nHost: y\r\n\r\n", 1000), "!400");
}

// RFC 9112 §7.1: chunked, its extensions, its trailers, cut anywhere
TEST(HttpParser_Tests, Chunked_7_1) {
    const std::string body = "5\r\nhello\r\n1;name\r\n \r\n6;a=b;c=\"q\\\"t\"\r\nworld!\r\n0\r\nX-Sum: 42\r\n\r\n";
    for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(1000)}) {
        for (size_t room : {size_t(1), size_t(4), size_t(100)}) {
            headers t;
            EXPECT_EQ(dechunk(body, piece, room, &t), "hello world!") << piece << " " << room;
            EXPECT_EQ(t.get("x-sum"), "42");
        }
    }
    // cut at every byte: incomplete, never wrong, until the last
    for (size_t cut = 0; cut < body.size(); ++cut) {
        auto r = dechunk(body.substr(0, cut), 3);
        EXPECT_EQ(r[0], '?') << cut << " " << r;
    }
    EXPECT_EQ(dechunk("0\r\n\r\n", 1), "");
    EXPECT_EQ(dechunk("0\r\n\r\nGET", 1000), "|rest:GET");                  // the next request left alone
    EXPECT_EQ(dechunk("A\r\n0123456789\r\n0\r\n\r\n", 2), "0123456789");     // hex, both cases
    EXPECT_EQ(dechunk("a\r\n0123456789\r\n0\r\n\r\n", 2), "0123456789");
    EXPECT_EQ(dechunk("0003\r\nabc\r\n0\r\n\r\n", 2), "abc");                // leading zeros
    // the extensions' grammar
    EXPECT_EQ(dechunk("3 ; a = b\r\nabc\r\n0\r\n\r\n", 5), "abc");         // BWS
    EXPECT_EQ(dechunk("3;=b\r\nabc\r\n0\r\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("3;a=\r\nabc\r\n0\r\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("3;a=\"b\r\nabc\r\n0\r\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("3;a b\r\nabc\r\n0\r\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("3;a=\"\x01\"\r\nabc\r\n0\r\n\r\n", 5), "!400");
    // the line bound (4 KB), Go's since 2023
    EXPECT_EQ(dechunk("3;a=" + std::string(4100, 'b') + "\r\nabc\r\n0\r\n\r\n", 64), "!400");
    EXPECT_EQ(dechunk("3;a=" + std::string(4000, 'b') + "\r\nabc\r\n0\r\n\r\n", 64), "abc");
    // trailers: fields, strictly CRLF, bounded
    EXPECT_EQ(dechunk("0\r\nA: b\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("0\r\n A: b\r\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("0\r\nA b\r\n\r\n", 5), "!400");
    EXPECT_EQ(dechunk("0\r\n" + std::string(40000, 'a') + ": b\r\n\r\n", 1000), "!400");
}

// RFC 9112 §4 and §6.3: the response side
TEST(HttpParser_Tests, Responses) {
    auto parse = [](const std::string& head, StatusLine& line, headers& h) {
        return parse_response_head(string(head), line, h);
    };
    StatusLine l;
    headers h;
    EXPECT_EQ(parse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", l, h), 0);
    EXPECT_EQ(l.status, 200);
    EXPECT_EQ(l.minor, 1);
    headers h2;
    EXPECT_EQ(parse("HTTP/1.0 404 \r\n\r\n", l, h2), 0);                    // an empty reason
    EXPECT_EQ(l.status, 404);
    headers h3;
    EXPECT_EQ(parse("HTTP/1.1 204\r\n\r\n", l, h3), 0);                     // no reason, no space
    for (auto bad : {"HTTP/1.1 20 OK\r\n\r\n", "HTTP/1.1 2000 OK\r\n\r\n", "HTTP/2 200 OK\r\n\r\n", "HTTP/1.1  200 OK\r\n\r\n",
                     "HTTP/1.1 099 X\r\n\r\n", "HTTP/1.1 200OK\r\n\r\n", "HTTP/1.1 200 O\x01K\r\n\r\n", "HTTP/1.1 200 OK\r\nA : b\r\n\r\n"}) {
        headers x;
        EXPECT_NE(parse(bad, l, x), 0) << bad;
    }
    auto framing = [](const std::string& fields, int status, bool head, BodyFraming& f) {
        headers x;
        StatusLine sl;
        EXPECT_EQ(parse_response_head(string("HTTP/1.1 " + std::to_string(status) + " X\r\n" + fields + "\r\n"), sl, x), 0);
        return response_framing(x, status, head, f);
    };
    BodyFraming f;
    EXPECT_TRUE(framing("Content-Length: 5\r\n", 200, false, f));
    EXPECT_EQ(f.kind, Framing::length);
    EXPECT_TRUE(framing("Content-Length: 5\r\n", 200, true, f));            // HEAD: none
    EXPECT_EQ(f.kind, Framing::none);
    EXPECT_TRUE(framing("Content-Length: 5\r\n", 204, false, f));
    EXPECT_EQ(f.kind, Framing::none);
    EXPECT_TRUE(framing("Transfer-Encoding: chunked\r\n", 304, false, f));
    EXPECT_EQ(f.kind, Framing::none);
    EXPECT_TRUE(framing("", 200, false, f));                                 // to the close
    EXPECT_EQ(f.kind, Framing::until_close);
    EXPECT_TRUE(framing("Transfer-Encoding: chunked\r\n", 200, false, f));
    EXPECT_EQ(f.kind, Framing::chunked);
    EXPECT_FALSE(framing("Transfer-Encoding: chunked\r\nContent-Length: 5\r\n", 200, false, f));
    EXPECT_FALSE(framing("Content-Length: 5\r\nContent-Length: 6\r\n", 200, false, f));
    EXPECT_FALSE(framing("Transfer-Encoding: gzip\r\n", 200, false, f));
}

// The head's end and its search resumed across reads
TEST(HttpParser_Tests, HeadEnd) {
    std::string h = "GET / HTTP/1.1\r\nHost: x\r\n\r\nrest";
    EXPECT_EQ(find_head_end(h.data(), h.size()), h.size() - 4);
    for (size_t cut = 0; cut < h.size() - 4; ++cut) {
        // the first part searched, then the whole from where it stopped
        EXPECT_EQ(find_head_end(h.data(), cut), 0u) << cut;
        EXPECT_EQ(find_head_end(h.data(), h.size(), head_search_resume(cut)), h.size() - 4) << cut;
    }
    std::string lf = "GET / HTTP/1.1\nHost: x\n\nrest";
    EXPECT_EQ(find_head_end(lf.data(), lf.size()), lf.size() - 4);
    std::string mixed = "GET / HTTP/1.1\r\nHost: x\n\r\nrest";
    EXPECT_EQ(find_head_end(mixed.data(), mixed.size()), mixed.size() - 4);
    EXPECT_EQ(leading_empty_lines("\r\n\n\r\nG", 6), 5u);
}

// A deterministic mutator over valid requests: whatever comes out, the
// parser answers 0 or one of its statuses, and an accepted head keeps the
// rules (one framing, a Host in 1.1, the length a number); the chunked
// decoder never overruns. The seed is in the name of the case.
TEST(HttpParser_Tests, MutatorSeed20260925) {
    const std::vector<std::string> corpus = {
        "GET /a?b=c HTTP/1.1\r\nHost: example.com\r\nUser-Agent: x\r\nAccept: */*\r\n\r\n",
        "POST /upload HTTP/1.1\r\nHost: [::1]:8080\r\nContent-Length: 5\r\nContent-Type: text/plain\r\n\r\nhello",
        "PUT /x HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\nT: v\r\n\r\n",
        "GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n",
        "OPTIONS * HTTP/1.1\r\nHost: h\r\n\r\n",
    };
    const std::string alphabet = std::string("\r\n :;,\t\"=-0123456789abcdefABCDEFxX/\\") + std::string(1, '\0') + "\x7F\x80\xFF";
    std::mt19937_64 rng(20260925);
    size_t accepted = 0, refused = 0;
    for (int round = 0; round < 20000; ++round) {
        std::string s = corpus[rng() % corpus.size()];
        int edits = 1 + int(rng() % 4);
        for (int e = 0; e < edits && !s.empty(); ++e) {
            size_t at = rng() % s.size();
            switch (rng() % 4) {
                case 0: s[at] = alphabet[rng() % alphabet.size()]; break;
                case 1: s.insert(at, 1, alphabet[rng() % alphabet.size()]); break;
                case 2: s.erase(at, 1); break;
                case 3: s.insert(at, s.substr(rng() % s.size(), 1 + rng() % 8)); break;
            }
        }
        auto c = check(s);
        if (c.status == -1) {
            continue;
        }
        ASSERT_TRUE(c.status == 0 || c.status == 400 || c.status == 501 || c.status == 505) << c.status;
        if (c.status) {
            ++refused;
            continue;
        }
        ++accepted;
        bool te = HeadersAccess::count(c.fields, "transfer-encoding") != 0;
        bool cl = HeadersAccess::count(c.fields, "content-length") != 0;
        EXPECT_FALSE(te && cl);
        if (te) {
            EXPECT_EQ(c.framing.kind, Framing::chunked);
            EXPECT_EQ(c.line.minor, 1);
            size_t skip = leading_empty_lines(s.data(), s.size());
            size_t end = find_head_end(s.data() + skip, s.size() - skip);
            auto r = dechunk(s.substr(skip + end), 1 + rng() % 5, 1 + rng() % 7);
            (void)r;
        }
        if (c.line.minor == 1) {
            EXPECT_EQ(HeadersAccess::count(c.fields, "host"), 1u);
        }
        for (auto& f : HeadersAccess::fields(c.fields)) {
            for (char ch : f.second.view()) {
                EXPECT_TRUE(field_value_char(uint8_t(ch)));
            }
            EXPECT_TRUE(is_token(f.first.view()));
        }
    }
    EXPECT_GT(accepted, 100u);
    EXPECT_GT(refused, 1000u);
}
