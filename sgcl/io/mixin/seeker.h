//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
            expected<uint64_t, error> tell() {
                return _self().seek(0, seek_from::current);
            }

            // The size, the position kept
            expected<uint64_t, error> size() {
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

            expected<void, error> rewind() {
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
            Derived& _self() noexcept {
                return static_cast<Derived&>(*this);
            }
        };
    }
}
