//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/coroutine.h"
#include "../error.h"

#include <cstdint>

namespace sgcl::io {
    enum class seek_from { begin, current, end };

    namespace mixin {
        // The mixin over Derived::seek(offset, from) -> result<uint64_t>: the
        // position after the seek
        template<class Derived>
        class seeker {
        public:
            result<uint64_t> tell() {
                return _self().seek(0, seek_from::current);
            }

            // The size, the position kept
            result<uint64_t> size() {
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

            result<void> rewind() {
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
