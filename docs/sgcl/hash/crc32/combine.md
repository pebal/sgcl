[sgcl](../../README.md) › [hash](../README.md) › [crc32](../crc32.md)

# sgcl::hash::crc32::combine

```cpp
static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept;
```

The CRC of A followed by B, given `first`, the CRC of A, `second`, the CRC of B, and `second_length`, the length of
B in bytes: zlib's `crc32_combine`, here for every CRC. With it a large buffer is hashed in pieces on several
threads or tasks and the pieces' CRCs joined, as pigz and a zip written from many threads do.

It is static and works on values: a CRC does not count its own length, which would cost every `update` an addition
for the sake of a call made once per piece. It is `constexpr`, and takes any length up to 2^64 − 1.

## Parameters

| Parameter | Description |
|---|---|
| `first` | the CRC of the first piece |
| `second` | the CRC of the second piece |
| `second_length` | the length of the second piece in bytes |

## Return value

The CRC of the two pieces, the first followed by the second.

## Complexity

Logarithmic in `second_length`: one multiplication modulo the polynomial for every bit of it that is set, over a
table of powers the compiler computes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

// A buffer hashed on four tasks, the pieces joined as pigz joins them
async::task<uint32_t> parallel_crc(slice<const byte> data) {
    constexpr int parts = 4;
    size_t part = data.size() / parts;
    vector<slice<const byte>> pieces;
    for (int i : range(parts)) {
        pieces.push_back(i + 1 < parts ? data.subslice(i * part, part) : data.subslice(i * part));
    }
    vector<async::task<uint32_t>> work;
    for (auto piece : pieces) {
        work.push_back(async::spawn([piece]() -> async::task<uint32_t> { co_return hash::crc32::of(piece); }));
    }
    auto sums = co_await async::when_all(std::move(work));
    uint32_t crc = sums[0];
    for (int i : range(1, parts)) {
        crc = hash::crc32::combine(crc, sums[i], pieces[i].size());
    }
    co_return crc;
}

int main() {
    vector<byte> data(1'000'003);
    for (int i : range(int(data.size()))) {
        data[i] = byte(i * 7);
    }
    uint32_t crc = async::run(parallel_crc(data));
    println("{:08x}", crc);
    println("{}", crc == hash::crc32::of(data));
}
```

Output:

```text
de35333e
true
```

## See also

- [resume](resume.md): going on from a saved CRC
- [sgcl::hash::crc32](../crc32.md)
