//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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

namespace sgcl::compress {
    namespace detail { using namespace sgcl::detail; }
    // What went wrong in compressed data or in an archive. One list for
    // every format of the module, as encoding has one for its formats; a
    // format uses the codes that mean something for it. The values start
    // at 1: an error_code of 0 is success, and a code of this list travels
    // as an error_code inside an io::error when a reader of the module is
    // read as a stream.
    enum class errc : uint8_t {
        corrupt = 1,         // data the format does not allow: a bad Huffman code, a distance before the start
        checksum,            // a CRC-32, an Adler-32 or a bzip2 block's CRC does not match
        unexpected_end,      // the data ends in the middle
        unsupported,         // a zip method, encryption, a sparse tar file, a zlib dictionary not given
        too_large,           // more than a limit allows: the size decompressed, a pax header
        invalid_header,      // a gzip, zip or tar header that cannot be read
        invalid_argument,    // what a writer cannot write: a name, a size, a second write past an entry's end
        dictionary_required, // a zlib stream made with a preset dictionary, read without it (or with another)
        io,                  // the source or the sink failed: io_error() says how
        password_required,   // encrypted data (a 7z entry or header) read without a password
        wrong_password       // encrypted data that does not decrypt under the password given (or is damaged: the two look alike)
    };

    namespace detail {
        class CompressCategory
        : public std::error_category {
        public:
            const char* name() const noexcept override {
                return "compress";
            }

            std::string message(int c) const override {
                switch (static_cast<errc>(c)) {
                    case errc::corrupt: return "corrupt data";
                    case errc::checksum: return "checksum mismatch";
                    case errc::unexpected_end: return "unexpected end of data";
                    case errc::unsupported: return "unsupported feature";
                    case errc::too_large: return "size limit exceeded";
                    case errc::invalid_header: return "invalid header";
                    case errc::invalid_argument: return "invalid argument";
                    case errc::dictionary_required: return "preset dictionary required";
                    case errc::io: return "input/output error";
                    case errc::password_required: return "password required";
                    case errc::wrong_password: return "wrong password";
                }
                return "unknown compress error";
            }
        };
    }

    inline const std::error_category& compress_category() noexcept {
        static const detail::CompressCategory instance;
        return instance;
    }

    inline error_code make_error_code(errc e) noexcept {
        return error_code(static_cast<int>(e), compress_category());
    }
}

template<>
struct std::is_error_code_enum<sgcl::compress::errc> : std::true_type {};

namespace sgcl::compress {
    // The error of every format of the module, one type under each
    // format's name (gzip::error, zip::error): the code, the byte of the
    // compressed input where it was found, and the error of the stream
    // when the input came from one and that failed. A value: copied,
    // compared, held in an expected.
    class error {
    public:
        error() = default;

        // The code at the offset; the detail, when given, is what
        // message() says in place of the code's own words ("gzip: CRC-32
        // mismatch", "zip: method 12 (bzip2)")
        error(errc code, uint64_t offset)
        : _code(code), _offset(offset) {
        }

        error(errc code, uint64_t offset, const string& detail)
        : _code(code), _offset(offset), _detail(detail) {
        }

        // The source or the sink failed at the offset
        error(const io::error& e, uint64_t offset)
        : _code(errc::io), _offset(offset), _io(e) {
        }

        errc code() const noexcept {
            return _code;
        }

        // Bytes from the start of the compressed input (of the archive)
        uint64_t offset() const noexcept {
            return _offset;
        }

        const optional<io::error>& io_error() const noexcept {
            return _io;
        }

        // "offset 1234: checksum mismatch", "offset 0: gzip: not a gzip
        // stream", "offset 512: input/output error: read: ..."
        string message() const {
            std::string m = "offset ";
            m += std::to_string(_offset);
            m += ": ";
            if (!_detail.empty()) {
                m.append(_detail.data(), _detail.size());
            } else {
                m += compress_category().message(static_cast<int>(_code));
            }
            if (_io) {
                m += ": ";
                auto t = _io->message();
                m.append(t.data(), t.size());
            }
            return string(m);
        }

        friend bool operator==(const error& a, const error& b) noexcept {
            return a._code == b._code && a._offset == b._offset && a._detail == b._detail && a._io == b._io;
        }

    private:
        errc _code = errc::corrupt;
        uint64_t _offset = 0;
        string _detail;
        optional<io::error> _io;
    };

    namespace detail {
        // The error as a stream reports it: a reader of the module read
        // through io::reader fails with an io::error of the compress
        // category ("read gzip: checksum mismatch"), the one of its source
        // passed on as it was
        inline io::error to_io_error(const error& e, const char* format) {
            if (e.io_error()) {
                return *e.io_error();
            }
            return io::error(make_error_code(e.code()), "read", format);
        }
    }
}
