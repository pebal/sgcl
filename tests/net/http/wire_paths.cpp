//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::http: the two ways a Wire reads a head (wire.h: read_head). Over a
// socket it tries the connection and waits for its readiness in its own
// frame (ConnImpl::try_read, raw_readable); over the ends in memory, which
// cannot be tried, it reads the waiting way (fill). The same requests,
// pipelined, cut into pieces of 1 to 7 bytes (a piece a write, the socket
// without Nagle and a pause between writes, so that the reads get them as
// pieces), read and parsed on both ways: the method, the target, the fields
// and the body the same, request by request.
#include "tests/types.h"
#include "sgcl/net/http/http.h"

#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace sgcl;
using namespace std::chrono_literals;

namespace {
    namespace hd = sgcl::net::http::detail;

    const std::string Requests =
        "GET /a?x=1 HTTP/1.1\r\nHost: h\r\nAccept: */*\r\n\r\n"
        "POST /b HTTP/1.1\r\nHost: h\r\nContent-Length: 11\r\n\r\nhello world"
        "\r\n\r\nPUT /c HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\nX-T: 1\r\n\r\n"
        "GET /d HTTP/1.1\r\nHost: h\r\nX-Long: " + std::string(12000, 'x') + "\r\n\r\n"
        "DELETE /e/f HTTP/1.0\r\nHost: h\r\nConnection: close\r\n\r\n";

    // The requests a Wire reads, each summed up in one line of text
    async::task<std::string> read_all(tracked_ptr<hd::Wire> wire) {
        std::string out;
        for (;;) {
            auto head = co_await wire->read_head(64 * 1024);
            if (!head) {
                out += "error " + std::string(head.error().message().c_str()) + "\n";
                co_return out;
            }
            if (!*head) {
                co_return out;
            }
            hd::RequestLine line;
            net::http::headers fields;
            hd::BodyFraming framing;
            string text = **head;
            int refused = hd::check_request_head(text, line, fields, framing);
            out += "refused " + std::to_string(refused) + " ";
            out += std::string(text.view().substr(line.method_at, line.method_size)) + " ";
            out += std::string(text.view().substr(line.target_at, line.target_size)) + " 1." + std::to_string(line.minor) + " |";
            for (auto f : fields) {
                out += std::string(f.first.view()) + "=" + std::to_string(f.second.size()) + ";";
            }
            if (framing.kind != hd::Framing::none) {
                tracked_ptr body = make_tracked<hd::Body>(wire, framing, 0, false);
                auto all = co_await body->read_everything();
                if (!all) {
                    out += " body error";
                    co_return out;
                }
                out += " body " + std::string(reinterpret_cast<const char*>(all->data()), all->size());
            }
            out += "\n";
        }
    }

    // The requests cut into pieces of 1 to 7 bytes, the cuts the same for
    // both ways (one seed)
    std::vector<std::string> pieces() {
        std::mt19937 rng(20260927);
        std::vector<std::string> out;
        for (size_t at = 0; at < Requests.size();) {
            size_t n = std::min<size_t>(1 + rng() % 7, Requests.size() - at);
            out.push_back(Requests.substr(at, n));
            at += n;
        }
        return out;
    }

    void send_pieces(const net::connection& c, bool pause) {
        for (auto& p : pieces()) {
            ASSERT_TRUE(c.write(string(std::string_view(p))));
            if (pause) {
                std::this_thread::sleep_for(20us);
            }
        }
        (void)c.close();
    }
}

TEST(HttpWirePaths_Tests, ASocketAndTheEndsInMemoryReadTheSameRequests) {
    std::string by_socket, in_memory;
    {
        auto l = net::tcp::listen("127.0.0.1:0");
        ASSERT_TRUE(l);
        auto accepting = async::spawn(l->async_accept());
        auto client = net::tcp::connect(l->local_endpoint());
        ASSERT_TRUE(client);
        ASSERT_TRUE(client->set_no_delay(true));
        auto server = accepting.wait();
        ASSERT_TRUE(server);
        tracked_ptr wire = make_tracked<hd::Wire>(*server);
        auto reading = async::spawn(read_all(wire));
        std::thread writer([&] { send_pieces(*client, true); });
        by_socket = reading.wait();
        writer.join();
        (void)server->close();
        (void)l->close();
    }
    {
        auto [a, b] = net::connection::in_memory();
        tracked_ptr wire = make_tracked<hd::Wire>(b);
        auto reading = async::spawn(read_all(wire));
        std::thread writer([&] { send_pieces(a, false); });
        in_memory = reading.wait();
        writer.join();
        (void)b.close();
    }
    EXPECT_EQ(std::count(by_socket.begin(), by_socket.end(), '\n'), 5);
    EXPECT_EQ(by_socket, in_memory);
    EXPECT_NE(by_socket.find("POST /b 1.1 |Host=1;Content-Length=2; body hello world"), std::string::npos) << by_socket;
    EXPECT_NE(by_socket.find("body hello world\n"), std::string::npos);
}
