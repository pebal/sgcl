//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "frames.h"
#include "image.h"
#include "options.h"
#include "detail/gif_decoder.h"
#include "detail/input.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../io/stream.h"

#include <chrono>

namespace sgcl::codec {
    namespace detail {
        // The frames of a GIF over an input of MemoryInput's shape; the
        // bytes (their owner held) or the stream kept with it
        template<class Input>
        struct GifFrames final : FramesState {
            slice<const byte> data;       // the bytes read from memory (empty for a stream)
            Input input;
            decode_options options;
            GifDecoder<Input> decoder;
            optional<error> failed;
            bool ended = false;
            bool given = false;     // a frame given: a file of none is an error at its end, as decode's
            bool reading = false;   // in a read of the stream; still set after one that threw

            template<class Source>
            SGCL_INLINE_HOT GifFrames(const slice<const byte>& d, const Source& source, const decode_options& o) noexcept
            : data(d), input(source), options(o), decoder(input, options) {
            }

            // A read of the stream that threw leaves the decoder in the
            // middle of a block: the reading stops there, errc::io after it
            expected<optional<frame>, error> next() noexcept(NothrowInput<Input>) override {
                if constexpr (NothrowInput<Input>) {
                    return _next();
                } else {
                    if (reading && !failed) {
                        failed = error(errc::io, input.offset(), "gif: a read of the stream threw before");
                    }
                    reading = true;
                    auto r = _next();
                    reading = false;
                    return r;
                }
            }

        private:
            expected<optional<frame>, error> _next() noexcept(NothrowInput<Input>) {
                if (failed) {
                    return unexpected(*failed);
                }
                if (ended) {
                    return optional<frame>();
                }
                switch (decoder.next()) {
                    case GifDecoder<Input>::Step::frame: {
                        plays = decoder.plays();
                        given = true;
                        const auto ms = std::chrono::milliseconds(int64_t(decoder.delay()) * 10);
                        return optional<frame>(frame{decoder.picture(), duration(ms)});
                    }
                    case GifDecoder<Input>::Step::end:
                        plays = decoder.plays();
                        if (!given) {
                            // as gif_first: no image in the file
                            failed = error(errc::corrupt, input.offset(), "gif: no image");
                            return unexpected(*failed);
                        }
                        ended = true;
                        return optional<frame>();
                    default:
                        failed = *decoder.failure();
                        return unexpected(*failed);
                }
            }
        };

        template<class Input, class Source>
        expected<frames, error> gif_frames(const slice<const byte>& data, const Source& source, const decode_options& o) noexcept(NothrowInput<Input>) {
            if (o.want && !valid(*o.want)) {
                return unexpected(error(errc::invalid_argument, 0, "gif: decode_options.want outside the list"));
            }
            auto s = make_tracked<GifFrames<Input>>(data, source, o);
            if (!s->decoder.start()) {
                return unexpected(*s->decoder.failure());
            }
            s->width = s->decoder.width();
            s->height = s->decoder.height();
            s->plays = s->decoder.plays();
            tracked_ptr<FramesState> state = std::move(s);
            return FramesAccess::make(state);
        }

        template<class Input>
        expected<image, error> gif_first(Input& in, const decode_options& o) noexcept(NothrowInput<Input>) {
            if (o.want && !valid(*o.want)) {
                return unexpected(error(errc::invalid_argument, 0, "gif: decode_options.want outside the list"));
            }
            // on the stack: its error holds tracked words (Rule 1)
            GifDecoder<Input> d(in, o);
            if (!d.start()) {
                return unexpected(*d.failure());
            }
            switch (d.next()) {
                case GifDecoder<Input>::Step::frame:
                    return d.picture();
                case GifDecoder<Input>::Step::end:
                    return unexpected(error(errc::corrupt, in.offset(), "gif: no image"));
                default:
                    return unexpected(*d.failure());
            }
        }
    }

    // GIF (GIF87a and GIF89a): every frame on the whole canvas, with its
    // transparency and disposal (detail/gif_decoder.h), the loop count of
    // NETSCAPE2.0. decode gives the first frame as an image, frames all of
    // them; each rgba8 unless decode_options.want asks for another format.
    class gif {
    public:
        // The first frame, the file in memory read in place
        SGCL_INLINE_HOT static expected<image, error> decode(const slice<const byte>& data, const decode_options& o = {}) noexcept {
            detail::MemoryInput in(data);
            return detail::gif_first(in, o);
        }

        // The first frame, from a stream read as far as it goes
        SGCL_INLINE_HOT static expected<image, error> decode(const io::reader& in, const decode_options& o = {}) {
            detail::ReaderInput source(in);
            return detail::gif_first(source, o);
        }

        // Every frame, read one by one as next() asks. The bytes are held
        // while the frames live (a slice of unmanaged memory must outlive them)
        SGCL_INLINE_HOT static expected<codec::frames, error> frames(const slice<const byte>& data, const decode_options& o = {}) noexcept {

            return detail::gif_frames<detail::MemoryInput>(data, data, o);
        }

        // Every frame from a stream, read as next() asks: memory the canvas
        // and a block of the stream, not the file
        SGCL_INLINE_HOT static expected<codec::frames, error> frames(const io::reader& in, const decode_options& o = {}) {
            return detail::gif_frames<detail::ReaderInput>(slice<const byte>(), in, o);
        }
    };
}
