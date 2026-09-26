# sgcl::hash::adler32

```cpp
#include "sgcl/hash/adler32.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class adler32;   // RFC 1950, the checksum of a zlib stream (Go's hash/adler32)
}
```

Adler-32: two sums modulo 65521, the largest prime below 2^16. `a` is one plus the bytes and `b` is the sum of every `a` along the way; the checksum is `b` in the high half and `a` in the low. It is what a zlib stream ends with, and cheaper to compute than a CRC where there is no CRC instruction, at the price of weak checking on short inputs.

## Members

```cpp
static constexpr size_t digest_size = 4;
static constexpr size_t block_size = 4;

adler32() noexcept;                            // the initial state: the checksum of nothing is 1
static adler32 resume(uint32_t value) noexcept;   // going on from the checksum of what came before

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
uint32_t value() const noexcept;
array<byte, 4> digest() const noexcept;   // the checksum, the most significant byte first: what zlib writes
void reset() noexcept;

static uint32_t of(/* bytes or text */) noexcept;
static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

```cpp
using namespace sgcl;

hash::adler32::of("Wikipedia");   // 0x11E60398
hash::adler32::of("");            // 1
```

A value given to the constructor whose halves are 65521 or more is not a checksum; each half is taken modulo 65521, so that the sums never start above what the loop's bound allows.

## combine

`combine(first, second, n)` is the checksum of A followed by B from the two checksums and `n`, the length of B: zlib's `adler32_combine`. Going through B from A's sums instead of from (1, 0) adds `a1 − 1` to every `a` on the way, so `a = a1 + a2 − 1` and `b = b1 + b2 + n·(a1 − 1)`, modulo 65521: a few arithmetic operations, whatever `n`.

## The loop

The bytes go in blocks of 32. Over a block, `a` grows by the sum of its bytes and `b` by 32 times the `a` it came in with plus Σ (32 − i)·xᵢ, and both sums are independent of each other and of the running values, so the inner loop is two plain reductions that the compiler vectorizes by itself; nothing here is an intrinsic. The modulo is taken once every 5536 bytes, the largest multiple of 32 within zlib's bound of 5552, where every sum still fits 32 bits even with every byte 0xFF and both sums one below the modulus coming in — a `static_assert` in the header works the worst case out.

## Example

```cpp
#include "sgcl/hash/adler32.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

using namespace sgcl;

int main() {
    io::stdout.write(txt::format("{:08x}\n", hash::adler32::of("Wikipedia")));
    // two pieces hashed apart, then joined
    uint32_t a = hash::adler32::of("Wiki");
    uint32_t b = hash::adler32::of("pedia");
    io::stdout.write(txt::format("{:08x}\n", hash::adler32::combine(a, b, 5)));
}
```

Output:

```text
11e60398
11e60398
```

## See also

[The module](README.md); [`crc32`](crc32.md), gzip's checksum where Adler-32 is zlib's; [`mixin::hasher`](hasher.md).
