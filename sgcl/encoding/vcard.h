//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "content_line.h"
#include "error.h"
#include "detail/content_reader.h"
#include "detail/files.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "../io/functions.h"
#include "../io/stream.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl::encoding {
    namespace detail {
        struct VcardNode;
        struct VcardAccess;
    }

    // One vCard (RFC 6350, 4.0; 3.0 of RFC 2426 read as well): its
    // properties in order, groups and parameters kept. Immutable, one word
    // shared by copying. Read by parse (one card) and parse_all (an address
    // book), written by to_string.
    class vcard {
    public:
        using error = encoding::error;
        using property = content_line;

        // What a parse accepts
        struct options {
            size_t max_size = size_t(64) << 20;   // the text's bytes
        };

        // A card of VERSION:4.0 alone
        vcard() noexcept;

        // A text of exactly one card
        static expected<vcard, error> parse(const string& text) noexcept;
        static expected<vcard, error> parse(const string& text, const options& o) noexcept;
        static expected<vcard, error> parse(const io::reader& in);
        static expected<vcard, error> parse(const io::reader& in, const options& o);
        static async::task<expected<vcard, error>> async_parse(io::reader in) noexcept;
        static async::task<expected<vcard, error>> async_parse(io::reader in, options o) noexcept;

        // Every card of a text: an address book
        static expected<vector<vcard>, error> parse_all(const string& text) noexcept;
        static expected<vector<vcard>, error> parse_all(const string& text, const options& o) noexcept;

        // The card of a file, every card of a file
        static expected<vcard, error> load(const string& path);
        static expected<vector<vcard>, error> load_all(const string& path);
        static async::task<expected<vcard, error>> async_load(string path) noexcept;

        // to_string into a file, made or written over
        expected<void, error> save(const string& path) const;
        async::task<expected<void, error>> async_save(string path) const noexcept;

        // BEGIN:VCARD, VERSION first, the other properties in order,
        // END:VCARD, each line folded at 75 octets and ended by CRLF
        string to_string() const;

        // VERSION's value: "4.0", "3.0"; empty without one
        string version() const noexcept;

        // FN's text: the name to show
        string formatted_name() const noexcept;

        slice<const property> properties() const noexcept;

        // The first property of the name (in any case), every one
        optional<property> property_of(const string& name) const noexcept;
        vector<property> properties_of(const string& name) const noexcept;

        // The first property's text() (FN, ORG, NOTE), or the fallback
        string text(const string& name, const string& fallback) const noexcept;

        // New versions: a property added at the end, every property of a
        // line's name replaced by it, every property of a name taken out
        vcard add(const property& p) const noexcept;
        vcard set(const property& p) const noexcept;
        vcard erase(const string& name) const noexcept;

        // The same properties in the same order
        friend bool operator==(const vcard& a, const vcard& b) noexcept;

    private:
        friend struct detail::VcardAccess;

        tracked_ptr<const detail::VcardNode> _node;

        const detail::VcardNode& _n() const noexcept;
    };

    namespace detail {
        struct VcardNode {
            vector<content_line> properties;
        };

        struct VcardAccess {
            static vcard make(vector<content_line> properties) noexcept {
                auto n = make_tracked<VcardNode>();
                n->properties = std::move(properties);
                vcard c;
                c._node = std::move(n);
                return c;
            }
        };
    }

    inline vcard::vcard() noexcept
    : _node([] {
          auto n = make_tracked<detail::VcardNode>();
          n->properties.push_back(content_line("VERSION", "4.0"));
          return n;
      }()) {
    }

    inline const detail::VcardNode& vcard::_n() const noexcept {
        return *_node;
    }

    inline slice<const vcard::property> vcard::properties() const noexcept {
        return _n().properties.as_slice();
    }

    inline optional<vcard::property> vcard::property_of(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        for (const property& p : _n().properties) {
            if (p.name().view() == u) {
                return p;
            }
        }
        return nullopt;
    }

    inline vector<vcard::property> vcard::properties_of(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        vector<property> out;
        for (const property& p : _n().properties) {
            if (p.name().view() == u) {
                out.push_back(p);
            }
        }
        return out;
    }

    inline string vcard::text(const string& name, const string& fallback) const noexcept {
        auto p = property_of(name);
        return p ? p->text() : fallback;
    }

    inline string vcard::version() const noexcept {
        auto p = property_of("VERSION");
        return p ? p->value() : string();
    }

    inline string vcard::formatted_name() const noexcept {
        return text("FN", string());
    }

    inline vcard vcard::add(const property& p) const noexcept {
        vector<property> props(_n().properties.begin(), _n().properties.end());
        props.push_back(p);
        return detail::VcardAccess::make(props);
    }

    inline vcard vcard::set(const property& p) const noexcept {
        vector<property> props;
        bool placed = false;
        for (const property& q : _n().properties) {
            if (q.name() == p.name()) {
                if (!placed) {
                    props.push_back(p);
                    placed = true;
                }
            } else {
                props.push_back(q);
            }
        }
        if (!placed) {
            props.push_back(p);
        }
        return detail::VcardAccess::make(props);
    }

    inline vcard vcard::erase(const string& name) const noexcept {
        std::string u = detail::cl_upper(name.view());
        vector<property> props;
        for (const property& q : _n().properties) {
            if (q.name().view() != u) {
                props.push_back(q);
            }
        }
        return detail::VcardAccess::make(props);
    }

    inline bool operator==(const vcard& a, const vcard& b) noexcept {
        const auto& x = a._n().properties;
        const auto& y = b._n().properties;
        if (x.size() != y.size()) {
            return false;
        }
        for (size_t i = 0; i < x.size(); ++i) {
            if (!(x[i] == y[i])) {
                return false;
            }
        }
        return true;
    }

    inline expected<vcard, vcard::error> vcard::parse(const string& text) noexcept {
        return parse(text, options());
    }

    inline expected<vcard, vcard::error> vcard::parse(const string& text, const options& o) noexcept {
        auto all = parse_all(text, o);
        if (!all) {
            return unexpected<error>(std::move(all.error()));
        }
        if (all->size() != 1) {
            error e(errc::syntax, 0, string(all->empty() ? "no VCARD" : "more than one VCARD"));
            return unexpected<error>(std::move(e.locate(text)));
        }
        return (*all)[0];
    }

    inline expected<vector<vcard>, vcard::error> vcard::parse_all(const string& text) noexcept {
        return parse_all(text, options());
    }

    inline expected<vector<vcard>, vcard::error> vcard::parse_all(const string& text, const options& o) noexcept {
        detail::ContentLimits limits;
        limits.max_depth = 1;   // a card holds no components
        limits.max_size = o.max_size;
        auto tree = detail::content_read(text, true, limits);
        if (!tree) {
            return unexpected<error>(std::move(tree.error()));
        }
        vector<vcard> out;
        for (size_t r : tree->roots) {
            if (tree->nodes[r].name.view() != "VCARD") {
                error e(errc::syntax, 0, string("a component that is no VCARD: " + std::string(tree->nodes[r].name.view())));
                return unexpected<error>(std::move(e.locate(text)));
            }
            out.push_back(detail::VcardAccess::make(tree->nodes[r].lines));
        }
        return out;
    }

    inline expected<vcard, vcard::error> vcard::parse(const io::reader& in) {
        return parse(in, options());
    }

    inline expected<vcard, vcard::error> vcard::parse(const io::reader& in, const options& o) {
        auto all = io::read_all(in);
        if (!all) {
            error e(all.error(), 0);
            return unexpected<error>(std::move(e));
        }
        return parse(string(*all), o);
    }

    inline async::task<expected<vcard, vcard::error>> vcard::async_parse(io::reader in) noexcept {
        return async_parse(std::move(in), options());
    }

    inline async::task<expected<vcard, vcard::error>> vcard::async_parse(io::reader in, options o) noexcept {
        auto all = co_await io::async_read_all(in);
        if (!all) {
            error e(all.error(), 0);
            co_return unexpected<error>(std::move(e));
        }
        co_return parse(string(*all), o);
    }

    inline string vcard::to_string() const {
        std::string out = "BEGIN:VCARD\r\n";
        for (const property& p : _n().properties) {
            if (p.name().view() == "VERSION") {
                out.append(p.to_string().view());
            }
        }
        for (const property& p : _n().properties) {
            if (p.name().view() != "VERSION") {
                out.append(p.to_string().view());
            }
        }
        out += "END:VCARD\r\n";
        return string(out);
    }

    namespace detail {
        inline async::task<expected<void, vcard::error>> vcard_save_task(string path, vcard value) noexcept {
            co_return co_await async::spawn_blocking([path, value] { return value.save(path); });
        }
    }

    inline expected<vcard, vcard::error> vcard::load(const string& path) {
        return detail::with_file(path, [](const io::reader& in) { return vcard::parse(in); });
    }

    inline expected<vector<vcard>, vcard::error> vcard::load_all(const string& path) {
        return detail::with_file(path, [](const io::reader& in) -> expected<vector<vcard>, error> {
            auto all = io::read_all(in);
            if (!all) {
                error e(all.error(), 0);
                return unexpected<error>(std::move(e));
            }
            return vcard::parse_all(string(*all));
        });
    }

    inline async::task<expected<vcard, vcard::error>> vcard::async_load(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return vcard::load(path); });
    }

    inline expected<void, vcard::error> vcard::save(const string& path) const {
        return detail::save_document(path, to_string());
    }

    inline async::task<expected<void, vcard::error>> vcard::async_save(string path) const noexcept {
        return detail::vcard_save_task(std::move(path), *this);
    }
}
