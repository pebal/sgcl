//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::mqtt on any bytes; the first byte picks what the rest is:
//   0, 1  a packet read under MQTT 3.1.1 (0) or 5 (1): one that reads is
//         written and read back to the same fields
//   2     a topic and a filter: a valid topic matches itself, "#" matches
//         every topic that does not start with "$", a filter that matches is
//         valid or the topic is the filter itself
//   3, 4  a client's bytes to the broker (after a CONNECT of 3.1.1 or 5 of
//         the harness's, or raw with the second byte's low bit): the broker's
//         session runs to the end of the bytes and stops; every packet it
//         writes reads as MQTT of the session's version
// The decoder reads the input's own bytes or a malloc'd block of exactly
// them, never a managed copy: a read past the end is ASan's to see (the
// broker reads its own buffer, as with a connection).
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/mqtt_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/mqtt.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    namespace md = sgcl::net::mqtt::detail;

    // The bytes in a malloc'd block of exactly their size, never a managed
    // or a std::string's copy: a read past their end is ASan's to see
    class Exact {
    public:
        explicit Exact(std::string_view s)
        : _p(static_cast<char*>(std::malloc(s.size()))), _n(s.size()) {
            std::copy_n(s.data(), s.size(), _p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(_p);
        }

        std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

    private:
        char* _p;
        size_t _n;
    };

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void packet(std::string_view bytes, uint8_t level) {
        size_t header = 0, total = 0;
        if (md::mqtt_frame(bytes, header, total) != 1 || total > bytes.size()) {
            return;
        }
        md::MqttPacket p;
        uint8_t reason = 0;
        if (!md::mqtt_decode(uint8_t(bytes[0]), bytes.substr(header, total - header), level, p, reason)) {
            check(reason != 0);
            return;
        }
        uint8_t as = p.type == md::packet::connect ? (p.level >= 5 ? 5 : 4) : level;
        Exact encoded(md::mqtt_encode(p, as));
        std::string_view again = encoded.view();
        md::MqttPacket q;
        size_t h2 = 0, t2 = 0;
        check(md::mqtt_frame(again, h2, t2) == 1 && t2 == again.size());
        check(md::mqtt_decode(uint8_t(again[0]), again.substr(h2), as, q, reason));
        check(q.type == p.type && q.id == p.id && q.topic == p.topic && q.payload == p.payload && q.qos == p.qos && q.retain == p.retain);
        check(q.filters == p.filters && q.subs.size() == p.subs.size() && q.client_id == p.client_id && q.will_topic == p.will_topic);
        check(q.props.user == p.props.user && q.props.subscription_ids == p.props.subscription_ids);
        if (p.type == md::packet::suback || (p.type == md::packet::unsuback && as >= 5)) {
            check(q.reasons == p.reasons);
        }
    }

    void topics(std::string_view text) {
        size_t cut = text.find('\n');
        std::string_view topic = text.substr(0, cut);
        std::string_view filter = cut == std::string_view::npos ? std::string_view() : text.substr(cut + 1);
        if (md::mqtt_topic_valid(topic)) {
            check(md::mqtt_match(topic, topic));
            check(md::mqtt_match("#", topic) == (topic[0] != '$'));
        }
        if (md::mqtt_match(filter, topic) && filter != topic) {
            check(md::mqtt_filter_valid(filter) || !md::mqtt_topic_valid(topic) || filter.find_first_of("+#") != std::string_view::npos);
        }
        std::string_view group;
        (void)md::mqtt_share(filter, &group);
    }

    // A link of bytes held here: what the broker reads, what it writes kept
    class FuzzLink final : public md::MqttLink {
    public:
        std::string in;
        size_t at = 0;
        std::mutex lock;   // the broker's writer may still write when its session has ended
        std::string out;

        async::task<expected<size_t, io::error>> fill(std::string& buf) noexcept override {
            size_t n = std::min<size_t>(in.size() - at, 37);
            buf.append(in, at, n);
            at += n;
            co_return n;
        }

        async::task<expected<void, io::error>> send(const std::string& data) noexcept override {
            std::lock_guard g(lock);
            out += data;
            co_return expected<void, io::error>();
        }

        expected<void, io::error> close() noexcept override {
            at = in.size();
            return {};
        }

        void read_deadline(time_point) noexcept override {
        }

        net::endpoint remote() const noexcept override {
            return net::endpoint();
        }
    };

    void broker(std::string_view bytes, uint8_t flags, uint8_t level) {
        tracked_ptr link = make_tracked<FuzzLink>();
        if (!(flags & 1)) {
            md::MqttPacket c;
            c.type = md::packet::connect;
            c.level = level;
            c.clean_start = true;
            c.client_id = "fuzz";
            link->in = md::mqtt_encode(c, level);
        }
        link->in.append(bytes.data(), bytes.size());
        tracked_ptr impl = make_tracked<md::BrokerImpl>();
        tracked_ptr cfg = make_tracked<md::BrokerSettings>();
        cfg->on_error = [](const string&) {};
        cfg->maximum_packet_size = 4096;
        cfg->max_session_expiry = 0;   // no session outlives its connection: nothing left waiting
        impl->running.add();
        md::broker_serve(impl, cfg, tracked_ptr<md::MqttLink>(link)).wait();
        if (flags & 1) {
            return;   // a CONNECT of the input's: its version unknown here
        }
        // what the broker wrote: whole packets of the version
        std::string written;
        {
            std::lock_guard g(link->lock);
            written = link->out;
        }
        std::string_view out = written;
        uint8_t as = level;
        while (!out.empty()) {
            size_t header = 0, total = 0;
            check(md::mqtt_frame(out, header, total) == 1 && total <= out.size());
            md::MqttPacket p;
            uint8_t reason = 0;
            check(md::mqtt_decode(uint8_t(out[0]), out.substr(header, total - header), as, p, reason));
            out.remove_prefix(total);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    std::string_view text(reinterpret_cast<const char*>(data + 2), size - 2);
    switch (data[0] % 5) {
        case 0: packet(text, 4); break;
        case 1: packet(text, 5); break;
        case 2: topics(text); break;
        case 3: broker(text, data[1], 4); break;
        case 4: broker(text, data[1], 5); break;
    }
    return 0;
}
