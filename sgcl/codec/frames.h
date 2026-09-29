//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "image.h"
#include "../core/aliases.h"
#include "../core/detail/handle_word.h"
#include "../core/duration.h"
#include "../core/expected.h"
#include "../core/tracked_ptr.h"

#include <cstdint>

namespace sgcl::codec {
    // A frame of an animation: the whole canvas as it is shown, and how
    // long (as the file says: GIF in hundredths of a second, 0 as often as
    // not, which browsers show as 100 ms)
    struct frame {
        image picture;
        duration delay;
    };

    class frames;

    namespace detail {
        // The state of an animation being read: one kind per format (GIF,
        // animated WebP), behind one interface; a managed object
        struct FramesState {
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t plays = 1;

            virtual ~FramesState() = default;
            virtual expected<optional<frame>, error> next() = 0;
        };

        struct FramesAccess {
            static frames make(const tracked_ptr<FramesState>& s) noexcept;
        };
    }

    // The frames of an animation (GIF, animated WebP), read one by one as
    // they are asked for: each a new image of the whole canvas, what came
    // before composed under it as the format says. A handle of one word;
    // copies share the reading. What the file is read from lives while the
    // frames do (bytes given as a slice of unmanaged memory must outlive
    // them).
    class frames {
    public:
        // The next frame; nullopt after the last; an error of the data
        // where it is found (and again on every call after)
        expected<optional<frame>, error> next() {
            return _s->next();
        }

        // The canvas
        uint32_t width() const noexcept {
            return _s->width;
        }

        uint32_t height() const noexcept {
            return _s->height;
        }

        // How many times the animation plays: 0 forever, 1 once (a file
        // that says nothing). Known once the file's loop extension is read,
        // which is before the first frame in the files that have one.
        uint32_t loop_count() const noexcept {
            return _s->plays;
        }

    private:
        friend struct detail::FramesAccess;
        friend struct sgcl::detail::HandleWord;

        explicit frames(const tracked_ptr<detail::FramesState>& s) noexcept
        : _s(s) {
        }

        frames(sgcl::detail::FromWord, const tracked_ptr<detail::FramesState>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<detail::FramesState>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<detail::FramesState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::FramesState> _s;
    };

    namespace detail {
        inline frames FramesAccess::make(const tracked_ptr<FramesState>& s) noexcept {
            return frames(s);
        }
    }
}
