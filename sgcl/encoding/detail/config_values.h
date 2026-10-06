//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/detail/maker.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <system_error>

// What the configuration formats (dotenv, ini) share: their entries in one
// managed buffer, and the typed reading of a value's text
namespace sgcl::encoding::detail {
    using namespace sgcl::detail;

    // A managed buffer of copies of the n T's, its owner; none for n == 0
    template<class T>
    tracked_ptr<const void> config_buffer(const T* first, size_t n) noexcept {
        tracked_ptr<const void> owner;
        if (n) {
            auto u = unique_ptr<T>(Maker<T[]>::make_tracked_data(n));
            T* p = u.get();
            for (size_t i = 0; i < n; ++i) {
                Maker<T>::construct(p + i, first[i]);
            }
            owner = tracked_ptr<const void>(std::move(u));
        }
        return owner;
    }

    SGCL_INLINE_HOT std::string_view config_trim(std::string_view s) noexcept {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
            s.remove_suffix(1);
        }
        return s;
    }

    // A decimal integer with an optional sign, blanks around it passed over
    inline optional<int64_t> config_int(std::string_view s) noexcept {
        s = config_trim(s);
        if (!s.empty() && s.front() == '+') {
            s.remove_prefix(1);
            if (!s.empty() && s.front() == '-') {
                return nullopt;
            }
        }
        int64_t v = 0;
        auto r = std::from_chars(s.data(), s.data() + s.size(), v);
        if (s.empty() || r.ec != std::errc() || r.ptr != s.data() + s.size()) {
            return nullopt;
        }
        return v;
    }

    // A decimal float (inf and nan among them), blanks around it passed over
    inline optional<double> config_double(std::string_view s) noexcept {
        s = config_trim(s);
        if (!s.empty() && s.front() == '+') {
            s.remove_prefix(1);
            if (!s.empty() && s.front() == '-') {
                return nullopt;
            }
        }
        double v = 0;
        auto r = std::from_chars(s.data(), s.data() + s.size(), v);
        if (s.empty() || r.ec != std::errc() || r.ptr != s.data() + s.size()) {
            return nullopt;
        }
        return v;
    }

    // true, yes, on, 1 and false, no, off, 0, in any case (configparser's
    // getboolean)
    inline optional<bool> config_bool(std::string_view s) noexcept {
        s = config_trim(s);
        char low[6] = {};
        if (s.empty() || s.size() > 5) {
            return nullopt;
        }
        for (size_t i = 0; i < s.size(); ++i) {
            char c = s[i];
            low[i] = c >= 'A' && c <= 'Z' ? char(c | 32) : c;
        }
        std::string_view l(low, s.size());
        if (l == "true" || l == "yes" || l == "on" || l == "1") {
            return true;
        }
        if (l == "false" || l == "no" || l == "off" || l == "0") {
            return false;
        }
        return nullopt;
    }
}
