//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The brokers the pages of net::amqp and net::nats talk to when
// run_blocks.py checks them (--amqp, --nats): the tests' own minimal
// broker and server (tests/net/amqp/server.h, tests/net/nats/server.h) on
// a port of the loopback, which this program prints ("port N") and serves
// until it is killed.
//
//   run_blocks_servers amqp | nats
#include "tests/net/amqp/server.h"
#include "tests/net/nats/server.h"

#include <cstdio>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    std::string what = argc > 1 ? argv[1] : "";
    sgcl::tracked_ptr<amqp_test::Broker> broker;   // kept by this frame for as long as the program runs
    sgcl::tracked_ptr<nats_test::Server> server;
    if (what == "amqp") {
        broker = sgcl::make_tracked<amqp_test::Broker>();
        std::printf("port %u\n", unsigned(broker->port()));
    } else if (what == "nats") {
        server = sgcl::make_tracked<nats_test::Server>();
        std::printf("port %u\n", unsigned(server->port()));
    } else {
        std::fprintf(stderr, "usage: run_blocks_servers amqp | nats\n");
        return 2;
    }
    std::fflush(stdout);
    for (;;) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
}
