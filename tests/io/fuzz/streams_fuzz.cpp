//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The streams of io over any bytes cut into any pieces, against the bytes
// read at once. The source is a reader that hands the data out in pieces
// of the sizes the input gives (1..9436 bytes, so that reads cross the 8 KB
// block of a buffered_reader both ways), or an io::buffer holding it all.
// A buffered_reader over it takes a sequence of operations from the input
// — read, read_line, read_until, peek, read_byte, discard, read_all — each
// held to a model that is only a position in the data: what a line, a
// token, a peek must be there. A bound on the line (set_max_line) is
// refused exactly when the token is longer, and the token is skipped with
// its delimiter, wherever its end lies. Then, each over a fresh
// source: io::copy of a limit_reader into an io::buffer, a tee_reader read
// to the end with its copy in a second buffer, a multi_reader of the data
// cut in two, and lines() to the end: each must give the data (or its
// lines) as one read of the whole does.
//
// The input: a mode byte (bit 0: a line bound, bit 1: an io::buffer for
// the source of the operations), the bound, four piece sizes, the count
// of operations, two bytes an operation, then the data.
//
// Built with libFuzzer (tests/fuzz/run.sh tests/io/fuzz/streams_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/io/buffered.h"
#include "sgcl/io/stream.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The data handed out in pieces of the given sizes (never 0 before
    // the end); lives on the harness's stack, the reader holds a pointer
    struct Pieces {
        const uint8_t* p = nullptr;
        size_t n = 0;
        size_t at = 0;
        size_t sizes[4] = {1, 1, 1, 1};
        size_t k = 0;

        io::reader reader() {
            return io::reader([this](slice<byte> b) -> size_t {
                size_t m = std::min({b.size(), sizes[k++ % 4], n - at});
                std::memcpy(b.data(), p + at, m);
                at += m;
                return m;
            });
        }
    };

    bool same(const void* a, const uint8_t* b, size_t n) {
        return n == 0 || std::memcmp(a, b, n) == 0;
    }

    // The model: the reader's position in the data
    struct Model {
        const uint8_t* p;
        size_t n;
        size_t at = 0;

        size_t left() const {
            return n - at;
        }

        // The token up to the delimiter (its length without it, and
        // whether it was found)
        size_t token(char d, bool& found) const {
            auto q = static_cast<const uint8_t*>(std::memchr(p + at, uint8_t(d), left()));
            found = q != nullptr;
            return found ? size_t(q - (p + at)) : left();
        }
    };

    // One operation, the model moved as the reader must have moved
    bool step(const io::buffered_reader& r, Model& m, uint8_t op, uint8_t arg, size_t max_line) {
        switch (op % 8) {
        case 0: {   // read
            std::vector<uint8_t> b(size_t(arg) * 37 + 1);
            auto got = r.read(slice<byte>(reinterpret_cast<byte*>(b.data()), b.size()));
            check(got.has_value());
            if (m.left() == 0) {
                check(*got == 0);
            } else {
                check(*got >= 1 && *got <= std::min(b.size(), m.left()));
                check(same(b.data(), m.p + m.at, *got));
            }
            m.at += *got;
            return true;
        }
        case 1:     // read_line
        case 2: {   // read_until
            const bool line = op % 8 == 1;
            const char d = line ? '\n' : char(arg);
            bool found = false;
            const size_t len = m.token(d, found);
            auto got = line ? r.read_line() : r.read_until(d);
            if (max_line && len > max_line) {
                check(!got && got.error().code() == io::errc::line_too_long);
                m.at += len + (found ? 1 : 0);   // skipped whole
                return true;
            }
            check(got.has_value());
            if (m.left() == 0) {
                check(!*got);
                return true;
            }
            check(bool(*got));
            size_t want = len + (!line && found ? 1 : 0);
            if (line && want > 0 && m.p[m.at + want - 1] == '\r') {
                --want;
            }
            check((*got)->size() == want && same((*got)->data(), m.p + m.at, want));
            m.at += len + (found ? 1 : 0);
            return true;
        }
        case 3: {   // peek
            const size_t k = size_t(arg) * 40;
            auto got = r.peek(k);
            check(got.has_value());
            const size_t want = std::min({k, config::io_buffer_size, m.left()});
            check(got->size() == want && same(got->data(), m.p + m.at, want));
            return true;
        }
        case 4: {   // read_byte
            auto got = r.read_byte();
            check(got.has_value());
            if (m.left() == 0) {
                check(!*got);
            } else {
                check(bool(*got) && uint8_t(**got) == m.p[m.at]);
                ++m.at;
            }
            return true;
        }
        case 5: {   // discard
            const size_t k = size_t(arg) * 37;
            auto got = r.discard(k);
            check(got.has_value() && *got == std::min(k, m.left()));
            m.at += *got;
            return true;
        }
        case 6:     // buffered
            check(r.buffered() <= m.left());
            return true;
        default: {  // read_all: the rest
            auto got = io::read_all(r);
            check(got.has_value() && got->size() == m.left() && same(got->data(), m.p + m.at, m.left()));
            m.at = m.n;
            return true;
        }
        }
    }

    std::vector<std::string_view> model_lines(const uint8_t* p, size_t n) {
        std::vector<std::string_view> out;
        size_t at = 0;
        while (at < n) {
            auto q = static_cast<const uint8_t*>(std::memchr(p + at, '\n', n - at));
            size_t end = q ? size_t(q - p) : n;
            size_t len = end - at;
            if (len > 0 && p[at + len - 1] == '\r') {
                --len;
            }
            out.emplace_back(reinterpret_cast<const char*>(p + at), len);
            at = q ? end + 1 : n;
        }
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 7) {
        return 0;
    }
    const uint8_t mode = data[0];
    const size_t max_line = (mode & 1) ? size_t(data[1]) * 4 + 1 : 0;
    size_t sizes[4];
    for (int i = 0; i < 4; ++i) {
        sizes[i] = size_t(data[2 + i]) * 37 + 1;
    }
    const size_t ops = std::min<size_t>(data[6], (size - 7) / 2);
    const uint8_t* op = data + 7;
    const uint8_t* p = op + 2 * ops;
    const size_t n = size_t(data + size - p);

    auto pieces = [&](const uint8_t* q, size_t m) {
        Pieces s;
        s.p = q;
        s.n = m;
        std::copy(sizes, sizes + 4, s.sizes);
        return s;
    };

    // The operations
    {
        Pieces src = pieces(p, n);
        io::buffered_reader r = (mode & 2) ? io::buffered_reader(io::buffer(slice<const byte>(reinterpret_cast<const byte*>(p), n)))
                                           : io::buffered_reader(src.reader());
        r.set_max_line(max_line);
        Model m{p, n};
        for (size_t i = 0; i < ops; ++i) {
            if (!step(r, m, op[2 * i], op[2 * i + 1], max_line)) {
                break;
            }
        }
    }

    // copy of a limit_reader into a buffer
    {
        Pieces src = pieces(p, n);
        const uint64_t limit = uint64_t(data[1]) * 97;
        io::limit_reader lim(src.reader(), limit);
        io::buffer out;
        auto got = io::copy(out, lim);
        const size_t want = size_t(std::min<uint64_t>(limit, n));
        check(got.has_value() && *got == want);
        check(out.size() == want && same(out.data().data(), p, want));
    }

    // a tee_reader read to the end
    {
        Pieces src = pieces(p, n);
        io::buffer copy;
        io::tee_reader tee(src.reader(), copy);
        auto got = io::read_all(tee);
        check(got.has_value() && got->size() == n && same(got->data(), p, n));
        check(copy.size() == n && same(copy.data().data(), p, n));
    }

    // a multi_reader of the data cut in two
    {
        const size_t cut = n ? size_t(data[1]) * 131 % (n + 1) : 0;
        Pieces a = pieces(p, cut);
        Pieces b = pieces(p + cut, n - cut);
        io::multi_reader both(vector<io::reader>{a.reader(), b.reader()});
        auto got = io::read_all(both);
        check(got.has_value() && got->size() == n && same(got->data(), p, n));
    }

    // lines() to the end
    {
        Pieces src = pieces(p, n);
        io::buffered_reader r(src.reader());
        auto want = model_lines(p, n);
        size_t i = 0;
        for (auto line : r.lines()) {
            check(i < want.size() && line.size() == want[i].size() && same(line.data(), reinterpret_cast<const uint8_t*>(want[i].data()), line.size()));
            ++i;
        }
        check(i == want.size() && !r.last_error());
    }
    return 0;
}
