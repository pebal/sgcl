//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../req.h"

#include <cstdint>

namespace sgcl::io {
    namespace mixin {
        // The mixin over Derived::seek(offset, from) -> expected<uint64_t, error>: the
        // position after the seek
        template<class Derived>
        class seeker {
        public:
            expected<uint64_t, error> tell() noexcept(_nothrow_seek()) {
                return _self().seek(0, seek_from::current);
            }

            // The size, the position kept
            expected<uint64_t, error> size() noexcept(_nothrow_seek()) {
                auto here = _self().seek(0, seek_from::current);
                if (!here) {
                    return here;
                }
                auto end = _self().seek(0, seek_from::end);
                if (!end) {
                    return end;
                }
                auto back = _self().seek(static_cast<int64_t>(*here), seek_from::begin);
                if (!back) {
                    return back;
                }
                return end;
            }

            expected<void, error> rewind() noexcept(_nothrow_seek()) {
                auto r = _self().seek(0, seek_from::begin);
                if (!r) {
                    return detail::fail(r);
                }
                return {};
            }

        protected:
            seeker() = default;
            ~seeker() = default;

        private:
            // Whether Derived's seek cannot throw
            static constexpr bool _nothrow_seek() noexcept {
                return noexcept(expected<uint64_t, error>(std::declval<Derived&>().seek(int64_t(0), seek_from::begin)));
            }

            Derived& _self() noexcept {
                return static_cast<Derived&>(*this);
            }
        };
    }
}
