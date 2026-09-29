//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// detail::SmallVector driven by a sequence of operations read from the
// input, against std::vector as its model: after every operation the size
// and the contents must be the model's. Two vectors (so that a copy and a
// move go between them, in every pair of states: inline and inline, inline
// and block, block and block), each with a counting policy that fills
// every byte it is given back or asked to wipe with a pattern, so that an
// element read after the vector let go of it shows. Also a byte vector of
// 64 inline, the shape secret_bytes has.
//
// The input: operations of three bytes (what, which vector, an argument).
//
// Built with libFuzzer (tests/fuzz/run.sh tests/core/fuzz/small_vector_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/core/detail/small_vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace {
    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Poisoning {
        static void* allocate(size_t bytes) {
            return ::operator new(bytes);
        }

        static void deallocate(void* p, size_t bytes) noexcept {
            std::memset(p, 0xEE, bytes);
            ::operator delete(p, bytes);
        }

        static void wipe(void* p, size_t bytes) noexcept {
            std::memset(p, 0xEE, bytes);
        }
    };

    template<class T, size_t N>
    struct Pair {
        using V = sgcl::detail::SmallVector<T, N, Poisoning>;
        V v[2];
        std::vector<T> m[2];

        void same(int i) {
            check(v[i].size() == m[i].size());
            check(v[i].capacity() >= v[i].size());
            check(v[i].capacity() >= N);
            for (size_t k = 0; k < m[i].size(); ++k) {
                check(v[i][k] == m[i][k]);
            }
        }

        void step(uint8_t what, int i, uint8_t arg) {
            int j = 1 - i;
            T x = T(arg * 131 + what);
            switch (what % 10) {
            case 0:
            case 1:
                v[i].push_back(x);
                m[i].push_back(x);
                break;
            case 2: {
                size_t n = arg % (3 * N + 5);
                v[i].resize(n);
                m[i].resize(n);
                break;
            }
            case 3: {
                size_t n = m[i].empty() ? 0 : arg % (m[i].size() + 1);
                v[i].erase_front(n);
                m[i].erase(m[i].begin(), m[i].begin() + n);
                break;
            }
            case 4:
                v[i] = v[j];
                m[i] = m[j];
                break;
            case 5:
                v[i] = std::move(v[j]);
                m[i] = std::move(m[j]);
                m[j].clear();
                check(v[j].empty());
                break;
            case 6: {
                typename Pair::V made(std::move(v[i]));
                check(v[i].empty());
                v[i] = std::move(made);
                break;
            }
            case 7: {
                T more[7];
                size_t n = arg % 7;
                for (size_t k = 0; k < n; ++k) {
                    more[k] = T(x + k);
                }
                v[i].append(more, n);
                m[i].insert(m[i].end(), more, more + n);
                break;
            }
            case 8:
                v[i].clear();
                m[i].clear();
                break;
            case 9: {
                size_t n = arg % (4 * N + 3);
                v[i].reserve(n);
                check(v[i].capacity() >= n);
                break;
            }
            }
            same(i);
            same(j);
        }
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Pair<uint32_t, 4> words;
    Pair<uint8_t, 64> bytes;
    for (size_t k = 0; k + 3 <= size; k += 3) {
        uint8_t what = data[k];
        int i = data[k + 1] & 1;
        if (data[k + 1] & 2) {
            bytes.step(what, i, data[k + 2]);
        } else {
            words.step(what, i, data[k + 2]);
        }
    }
    return 0;
}
