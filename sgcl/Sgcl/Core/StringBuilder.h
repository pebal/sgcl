//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// StringBuilder: the scratch buffer a String is built in, appended to
// piece by piece and made into a String with ToString(). A std::string
// under the interface's names: unmanaged memory with no tracked pointer
// in it, so it lives anywhere and costs the collector nothing.
#pragma once

#include "String.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Sgcl {
    template<class CharT, class Traits = std::char_traits<CharT>>
    class BasicStringBuilder {
    public:
        using ValueType = CharT;
        using SizeType = size_t;
        using ViewType = std::basic_string_view<CharT, Traits>;
        using StringType = BasicString<CharT, Traits>;
        using InnerType = std::basic_string<CharT, Traits>;

        BasicStringBuilder() = default;

        explicit BasicStringBuilder(ViewType s)
        : _buffer(s) {
        }

        explicit BasicStringBuilder(InnerType s) noexcept
        : _buffer(std::move(s)) {
        }

        BasicStringBuilder& Append(CharT c) {
            _buffer.push_back(c);
            return *this;
        }

        BasicStringBuilder& Append(SizeType n, CharT c) {
            _buffer.append(n, c);
            return *this;
        }

        BasicStringBuilder& Append(ViewType s) {
            _buffer.append(s);
            return *this;
        }

        BasicStringBuilder& Append(const CharT* s) {
            _buffer.append(s);
            return *this;
        }

        BasicStringBuilder& Append(const StringType& s) {
            _buffer.append(s.View());
            return *this;
        }

        template<class T>
        requires std::is_arithmetic_v<T>
        BasicStringBuilder& Append(T v) {
            return Append(Sgcl::ToString(v).View());
        }

        template<class T>
        BasicStringBuilder& operator<<(const T& v) {
            return Append(v);
        }

        SizeType Length() const noexcept {
            return _buffer.size();
        }

        bool IsEmpty() const noexcept {
            return _buffer.empty();
        }

        void Reserve(SizeType n) {
            _buffer.reserve(n);
        }

        void Clear() noexcept {
            _buffer.clear();
        }

        // The characters so far, valid until the next Append
        ViewType View() const noexcept {
            return _buffer;
        }

        // The String: one managed object with the characters; the buffer stays as it is
        StringType ToString() const {
            return StringType(View());
        }

        InnerType& Inner() noexcept {
            return _buffer;
        }

        const InnerType& Inner() const noexcept {
            return _buffer;
        }

    private:
        InnerType _buffer;
    };

    using StringBuilder = BasicStringBuilder<char>;
    using WStringBuilder = BasicStringBuilder<wchar_t>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

