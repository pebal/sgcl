//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The tar reader on any bytes, three times: as they are; with every
// block's checksum made right (a mutated header gets past the checksum
// to its fields), fed whole; and the same fed three bytes at a time. Every
// entry's data is read (up to 1 MiB, the rest stepped over by next()).
// On the fixed input, every entry the reader gives and the writer takes
// is written again and read back: it must come back the same, with the
// same data (a trap otherwise).
#include "sgcl/compress/compress.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    namespace tar = compress::tar;

    struct memory {
        const uint8_t* p;
        size_t n;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            size_t k = std::min({step, b.size(), n - at});
            std::memcpy(b.data(), p + at, k);
            at += k;
            return k;
        }
    };

    // Every block that is not zeros given the checksum of what it holds
    void fix_checksums(std::vector<uint8_t>& d) {
        for (size_t b = 0; b + 512 <= d.size(); b += 512) {
            uint8_t* h = d.data() + b;
            bool zero = true;
            for (size_t i = 0; i < 512 && zero; ++i) {
                zero = h[i] == 0;
            }
            if (zero) {
                continue;
            }
            std::memset(h + 148, ' ', 8);
            unsigned sum = 0;
            for (size_t i = 0; i < 512; ++i) {
                sum += h[i];
            }
            char f[8];
            std::snprintf(f, sizeof f, "%06o", sum & 0777777);
            std::memcpy(h + 148, f, 6);
            h[154] = 0;
        }
    }

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // The same entry, the pax records in the order of their keys (as the
    // writer writes them)
    bool same(tar::entry a, tar::entry b) {
        auto by_key = [](const auto& x, const auto& y) { return x.first.view() < y.first.view(); };
        std::sort(a.pax.begin(), a.pax.end(), by_key);
        std::sort(b.pax.begin(), b.pax.end(), by_key);
        return a == b;
    }

    // The entry written by the writer and read back: the same entry and data
    void round_trip(const tar::entry& e, const std::string& data) {
        io::buffer sink;
        tar::writer w(sink);
        if (!w.write_header(e)) {
            return;   // one the writer refuses: an empty name, records past 1 MiB
        }
        check(bool(w.write(slice<const std::byte>(reinterpret_cast<const std::byte*>(data.data()), data.size()))));
        check(bool(w.close()));
        tar::reader r(sink);
        auto back = r.next();
        check(back && *back);
        check(same(**back, e));
        auto all = r.read_all();
        check(all && all->size() == data.size() && std::memcmp(all->data(), data.data(), data.size()) == 0);
        auto end = r.next();
        check(end && !*end);
    }

    void run(const uint8_t* data, size_t size, size_t step, bool trip) {
        memory m{data, size, step};
        tar::reader r(m);
        for (int k = 0; k < 256; ++k) {
            auto e = r.next();
            if (!e) {
                if (e.error().code() == compress::errc::unsupported) {
                    continue;
                }
                check(r.last_error().has_value());
                (void)e.error().message();
                break;
            }
            if (!*e) {
                break;
            }
            (void)(*e)->is_local();
            std::string got;
            std::byte buf[4096];
            bool whole = true;
            while (got.size() < (1u << 20)) {
                auto n = r.read(buf);
                if (!n) {
                    whole = false;
                    break;
                }
                if (*n == 0) {
                    break;
                }
                got.append(reinterpret_cast<const char*>(buf), *n);
            }
            if (whole) {
                check(got.size() <= (*e)->size);
                if (trip && got.size() == (*e)->size) {
                    round_trip(**e, got);
                }
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    run(data, size, size + 1, false);
    std::vector<uint8_t> fixed(data, data + size);
    fix_checksums(fixed);
    run(fixed.data(), fixed.size(), fixed.size() + 1, true);
    run(fixed.data(), fixed.size(), 3, false);
    return 0;
}
