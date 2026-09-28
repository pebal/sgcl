//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The zip reader on any bytes: the archive opened from memory, every
// entry read whole (under a limit) and through its reader
#include "sgcl/compress/compress.h"

#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    using namespace sgcl;
    // the writer: the input cut into entries by its own bytes, written,
    // and read back whole
    {
        io::buffer sink;
        compress::zip::writer w(sink);
        std::vector<std::string> parts;
        for (size_t at = 0, k = 0; at < size && k < 8; ++k) {
            size_t n = std::min<size_t>(size - at, 1 + data[at] * 7u);
            parts.emplace_back(reinterpret_cast<const char*>(data) + at, n);
            compress::zip::entry e;
            e.name = string("f" + std::to_string(k));
            e.method = data[at] & 1 ? compress::zip::method::store : compress::zip::method::deflate;
            auto ew = w.create(e);
            if (!ew || !ew->write(slice<const std::byte>(reinterpret_cast<const std::byte*>(parts.back().data()), n))) {
                __builtin_trap();
            }
            at += n;
        }
        if (!w.close()) {
            __builtin_trap();
        }
        auto back = compress::zip::archive::from(slice<const std::byte>(sink.data().data(), sink.size()));
        if (!back || back->entries().size() != parts.size()) {
            __builtin_trap();
        }
        for (size_t k = 0; k < parts.size(); ++k) {
            auto got = back->read(back->entries()[k]);
            if (!got || std::string(reinterpret_cast<const char*>(got->data()), got->size()) != parts[k]) {
                __builtin_trap();
            }
        }
    }
    auto a = compress::zip::archive::from(slice<const std::byte>(reinterpret_cast<const std::byte*>(data), size));
    if (!a) {
        return 0;
    }
    size_t k = 0;
    for (auto& e : a->entries()) {
        if (++k > 64) {
            break;
        }
        (void)e.is_local();
        (void)a->read(e, compress::limits{uint64_t(1) << 22});
        auto r = a->reader(e);
        if (r) {
            std::byte buf[1024];
            for (size_t guard = 0; guard < 4096; ++guard) {
                auto n = r->read(buf);
                if (!n || *n == 0) {
                    break;
                }
            }
        }
    }
    return 0;
}
