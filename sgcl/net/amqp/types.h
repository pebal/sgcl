//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../time/datetime.h"

#include <cstdint>
#include <string_view>

// The values of AMQP 0-9-1: a field table's values, a message's
// properties, what a consumer receives, what the declarations take
namespace sgcl::net::amqp {
    class field;

    // A field table (0-9-1 §4.2.5.5): names and values in their order, the
    // arguments of a declaration and the headers of a message
    using table = vector<pair<string, field>>;

    namespace detail {
        struct AmqpNest;
        class AmqpWriter;
    }

    // A value of a field table, of one of the types RabbitMQ writes and
    // reads: none (void), a boolean, the integers, the two floats, a
    // decimal, a string, bytes, a timestamp, an array, a table. A plain
    // value; made by its constructors (the common types) and by the named
    // ones (the exact type on the wire), read by as_*.
    class field {
    public:
        enum class kind : uint8_t { none, boolean, int8, uint8, int16, uint16, int32, uint32, int64, float32, float64, decimal, string, bytes, timestamp, array, table };

        field() noexcept {   // none: 'V'
        }

        field(bool v) noexcept
        : _kind(kind::boolean), _i(v) {
        }

        field(int32_t v) noexcept
        : _kind(kind::int32), _i(v) {
        }

        field(int64_t v) noexcept
        : _kind(kind::int64), _i(v) {
        }

        field(double v) noexcept
        : _kind(kind::float64), _d(v) {
        }

        field(const char* v)
        : _kind(kind::string), _s(v) {
        }

        field(const string& v) noexcept
        : _kind(kind::string), _s(v) {
        }

        field(const vector<field>& items);
        field(const table& t);

        static field int8(int8_t v) noexcept {
            return field(kind::int8, v);
        }

        static field uint8(uint8_t v) noexcept {
            return field(kind::uint8, v);
        }

        static field int16(int16_t v) noexcept {
            return field(kind::int16, v);
        }

        static field uint16(uint16_t v) noexcept {
            return field(kind::uint16, v);
        }

        static field uint32(uint32_t v) noexcept {
            return field(kind::uint32, v);
        }

        static field float32(float v) noexcept {
            field f;
            f._kind = kind::float32;
            f._d = v;
            return f;
        }

        // value / 10^scale
        static field decimal(uint8_t scale, int32_t value) noexcept {
            field f(kind::decimal, value);
            f._scale = scale;
            return f;
        }

        // A timestamp ('T'), in whole seconds
        static field timestamp(const time::datetime& t) noexcept {
            return field(kind::timestamp, t.unix());
        }

        // Bytes ('x'), not text
        static field bytes(const string& v) noexcept {
            field f(v);
            f._kind = kind::bytes;
            return f;
        }

        kind type() const noexcept {
            return _kind;
        }

        optional<bool> as_bool() const noexcept {
            if (_kind != kind::boolean) {
                return nullopt;
            }
            return _i != 0;
        }

        // Any of the integers
        optional<int64_t> as_int() const noexcept {
            switch (_kind) {
                case kind::int8: case kind::uint8: case kind::int16: case kind::uint16: case kind::int32: case kind::uint32: case kind::int64:
                    return _i;
                default:
                    return nullopt;
            }
        }

        // A float, a double, a decimal, an integer
        optional<double> as_double() const noexcept {
            if (_kind == kind::float32 || _kind == kind::float64) {
                return _d;
            }
            if (_kind == kind::decimal) {
                double d = double(_i);
                for (uint8_t i = 0; i < _scale; ++i) {
                    d /= 10;
                }
                return d;
            }
            if (auto i = as_int()) {
                return double(*i);
            }
            return nullopt;
        }

        // A string or bytes
        optional<string> as_string() const noexcept {
            if (_kind != kind::string && _kind != kind::bytes) {
                return nullopt;
            }
            return _s;
        }

        // A timestamp, in UTC
        optional<time::datetime> as_timestamp() const noexcept {
            if (_kind != kind::timestamp) {
                return nullopt;
            }
            return time::datetime::from_unix(_i, time::zone::utc());
        }

        optional<vector<field>> as_array() const;
        optional<table> as_table() const;

        // A decimal's scale
        uint8_t scale() const noexcept {
            return _scale;
        }

        friend bool operator==(const field& a, const field& b) noexcept;

    private:
        friend class detail::AmqpWriter;

        field(kind k, int64_t v) noexcept
        : _kind(k), _i(v) {
        }

        kind _kind = kind::none;
        uint8_t _scale = 0;
        union {
            int64_t _i = 0;
            double _d;
        };
        string _s;
        tracked_ptr<detail::AmqpNest> _nest;   // an array's or a table's items
    };

    namespace detail {
        struct AmqpNest {
            vector<field> items;
            table entries;
        };
    }

    inline field::field(const vector<field>& items)
    : _kind(kind::array), _nest(make_tracked<detail::AmqpNest>()) {
        _nest->items = items;
    }

    inline field::field(const table& t)
    : _kind(kind::table), _nest(make_tracked<detail::AmqpNest>()) {
        _nest->entries = t;
    }

    inline optional<vector<field>> field::as_array() const {
        if (_kind != kind::array) {
            return nullopt;
        }
        return _nest->items;
    }

    inline optional<table> field::as_table() const {
        if (_kind != kind::table) {
            return nullopt;
        }
        return _nest->entries;
    }

    inline bool operator==(const field& a, const field& b) noexcept {
        if (a._kind != b._kind) {
            return false;
        }
        switch (a._kind) {
            case field::kind::none: return true;
            case field::kind::float32: case field::kind::float64: return a._d == b._d;
            case field::kind::decimal: return a._scale == b._scale && a._i == b._i;
            case field::kind::string: case field::kind::bytes: return a._s == b._s;
            case field::kind::array: return a._nest->items == b._nest->items;
            case field::kind::table: return a._nest->entries == b._nest->entries;
            default: return a._i == b._i;
        }
    }

    // The value of a name in a table; none when the table lacks it
    inline optional<field> find(const table& t, const string& name) {
        for (auto& [n, v] : t) {
            if (n == name) {
                return v;
            }
        }
        return nullopt;
    }

    // How a broker keeps a message: in memory or on disk too
    enum class delivery_mode : uint8_t { none = 0, transient = 1, persistent = 2 };

    // The properties of a message (0-9-1 §4.2.6, the basic class's): what
    // it carries beside its body; empty, zero and none are not sent
    struct properties {
        string content_type;                    // "application/json"
        string content_encoding;                // "gzip"
        table headers;                          // the application's own; the headers exchange matches them
        amqp::delivery_mode delivery_mode = amqp::delivery_mode::none;
        uint8_t priority = 0;                   // 0..9
        string correlation_id;                  // of a reply: the request's message_id
        string reply_to;                        // the queue a reply goes to
        string expiration;                      // milliseconds as text, RabbitMQ's per-message TTL
        string message_id;
        optional<time::datetime> timestamp;     // in whole seconds
        string type;
        string user_id;                         // checked against the connection's user by RabbitMQ
        string app_id;

        friend bool operator==(const properties&, const properties&) noexcept = default;
    };

    // A message as a consumer or get receives it
    struct delivery {
        string body;                            // bytes in a string
        amqp::properties properties;
        string consumer_tag;                    // empty for get
        uint64_t delivery_tag = 0;              // what ack, nack and reject take
        bool redelivered = false;               // delivered before and not acked
        string exchange;
        string routing_key;
        uint32_t message_count = 0;             // get: the messages left in the queue
    };

    // A publication the broker gave back (basic.return): mandatory with no
    // queue to route it to
    struct returned {
        string body;
        amqp::properties properties;
        int reply_code = 0;                     // 312 no route
        string reply_text;
        string exchange;
        string routing_key;
    };

    // What declare_exchange takes
    struct exchange_options {
        string type = string("direct");         // "direct", "fanout", "topic", "headers", a plugin's
        bool passive = false;                   // only checked that it exists: not_found when it does not
        bool durable = false;                   // kept over a restart of the broker
        bool auto_delete = false;               // deleted when its last binding goes
        bool internal = false;                  // no publications of clients
        table arguments;                        // "alternate-exchange" and the like
    };

    // What declare_queue takes
    struct queue_options {
        bool passive = false;                   // only checked that it exists
        bool durable = false;                   // kept over a restart of the broker
        bool exclusive = false;                 // this connection's alone, deleted at its end
        bool auto_delete = false;               // deleted when its last consumer goes
        table arguments;                        // "x-message-ttl", "x-max-length", "x-queue-type"...
    };

    // What declare_queue gives: the name (the broker's for a queue declared
    // without one), the messages and the consumers it has
    struct queue_info {
        string name;
        uint32_t messages = 0;
        uint32_t consumers = 0;
    };

    // What consume takes
    struct consume_options {
        string tag;                             // empty: the broker names it
        bool no_ack = false;                    // acked by the broker as it sends
        bool exclusive = false;                 // the queue's only consumer
        table arguments;
    };

    // What publish takes beside the message
    struct publish_options {
        bool mandatory = false;                 // given back (receive_returned) when no queue takes it
    };
}
