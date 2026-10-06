//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/string.h"
#include "../io/error.h"

#include <cstdint>
#include <string>
#include <system_error>

namespace sgcl::codec {
    namespace detail { using namespace sgcl::detail; }
    // What went wrong in an image file. One list for every format of the
    // module; a format uses the codes that mean something for it. The
    // values start at 1: an error_code of 0 is success, and a code of this
    // list travels as an error_code inside an io::error when a decoder is
    // read as a stream.
    enum class errc : uint8_t {
        corrupt = 1,       // data the format does not allow: a bad chunk, a Huffman code with no symbol, a row past the image
        checksum,          // a PNG chunk's CRC-32 or the zlib stream's Adler-32 does not match
        unexpected_end,    // the data ends in the middle
        unsupported,       // valid data the module does not read: arithmetic-coded JPEG, 12-bit samples, not an image it knows
        too_large,         // more than limits allows: the pixels, the metadata
        invalid_argument,  // what the program asked makes no sense: decode_options.want outside the list, a HEIC quality outside 1..100,
                           // GIF's colors outside 2..256, an animation of no frame or frames of different sizes,
                           // an image the format cannot hold (a JPEG or GIF side past 65535, a PNG side past 2^31 - 1) or the system's encoder
                           // does not take (JPEG's quality outside 1..100 and subsampling outside the list are a thrown
                           // std::invalid_argument, a contract)
        io                 // the source or the sink failed: io_error() says how
    };

    namespace detail {
        class CodecCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "codec";
            }

            std::string message(int c) const noexcept override {
                switch (static_cast<errc>(c)) {
                    case errc::corrupt: return "corrupt image data";
                    case errc::checksum: return "checksum mismatch";
                    case errc::unexpected_end: return "unexpected end of data";
                    case errc::unsupported: return "unsupported feature";
                    case errc::too_large: return "size limit exceeded";
                    case errc::invalid_argument: return "invalid argument";
                    case errc::io: return "input/output error";
                }
                return "unknown codec error";
            }
        };
    }

    inline const std::error_category& codec_category() noexcept {
        static const detail::CodecCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), codec_category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::codec::errc> : std::true_type {};

namespace sgcl::codec {
    // The error of every format of the module: the code, the byte of the
    // input where it was found, and the error of the stream when the input
    // came from one and that failed. A value: copied, compared, held in an
    // expected.
    class error {
    public:
        // No code of the list (errc{}, 0) at offset 0: what an error is
        // before anything is assigned to it; message() says "no error"
        error() noexcept = default;

        // The code at the offset; the detail, when given, is what
        // message() says in place of the code's own words ("png: CRC-32 of
        // chunk IHDR", "jpeg: arithmetic coding")
        SGCL_INLINE_HOT error(errc code, uint64_t offset) noexcept
        : _code(code), _offset(offset) {
        }

        SGCL_INLINE_HOT error(errc code, uint64_t offset, const string& detail) noexcept
        : _code(code), _offset(offset), _detail(detail) {
        }

        // The source or the sink failed at the offset
        SGCL_INLINE_HOT error(const io::error& e, uint64_t offset) noexcept
        : _code(errc::io), _offset(offset), _io(e) {
        }

        // Copied without a throw: the words and the stream's error are
        // shared, not copied (optional's own copy is not declared noexcept)
        SGCL_INLINE_HOT error(const error& o) noexcept
        : _code(o._code), _offset(o._offset), _detail(o._detail), _io(o._io) {
        }

        error(error&&) noexcept = default;

        SGCL_INLINE_HOT error& operator=(const error& o) noexcept {
            _code = o._code;
            _offset = o._offset;
            _detail = o._detail;
            _io = o._io;
            return *this;
        }

        error& operator=(error&&) noexcept = default;

        SGCL_INLINE_HOT errc code() const noexcept {
            return _code;
        }

        // Bytes from the start of the input
        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return _offset;
        }

        SGCL_INLINE_HOT const optional<io::error>& io_error() const noexcept {
            return _io;
        }

        // "offset 1234: checksum mismatch", "offset 8: png: CRC-32 of chunk
        // IHDR", "offset 512: input/output error: read: ..."; "no error" for
        // an error of no code (a default one)
        string message() const noexcept {
            if (_code == errc{}) {
                return string("no error");
            }
            std::string m = "offset ";
            m += std::to_string(_offset);
            m += ": ";
            if (!_detail.empty()) {
                m.append(_detail.data(), _detail.size());
            } else {
                m += codec_category().message(static_cast<int>(_code));
            }
            if (_io) {
                m += ": ";
                auto t = _io->message();
                m.append(t.data(), t.size());
            }
            return string(m);
        }

        SGCL_INLINE_HOT friend bool operator==(const error& a, const error& b) noexcept {
            return a._code == b._code && a._offset == b._offset && a._detail == b._detail && a._io == b._io;
        }

    private:
        errc _code = errc{};
        uint64_t _offset = 0;
        string _detail;
        optional<io::error> _io;
    };

    namespace detail {
        // The error as a stream reports it: a decoder read through
        // io::reader fails with an io::error of the codec category ("read
        // png: checksum mismatch"), the one of its source passed on as it
        // was
        inline io::error to_io_error(const error& e, const char* format) noexcept {
            if (e.io_error()) {
                return *e.io_error();
            }
            return io::error(make_error_code(e.code()), "read", format);
        }
    }
}
