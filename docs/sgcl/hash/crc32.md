# sgcl::hash::crc32, sgcl::hash::crc32c

```cpp
#include "sgcl/hash/crc32.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class crc32;    // CRC-32/ISO-HDLC: zlib, gzip, zip, PNG, Ethernet (Go's crc32.IEEE)
    class crc32c;   // CRC-32/ISCSI, Castagnoli: iSCSI, ext4, Btrfs, SCTP, LevelDB, gRPC (Go's crc32.Castagnoli)
}
```

The two 32-bit CRCs of the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/) a program meets: both reflected, both starting from all ones and inverting the result. Two types rather than one with a polynomial, because a program that needs one of them needs that one by name. Each holds four bytes, its register, and has the [common shape](hasher.md) of the module.

## Members

```cpp
static constexpr size_t digest_size = 4;
static constexpr size_t block_size = 1;

crc32() noexcept;                              // the initial state: the CRC of nothing is 0
static crc32 resume(uint32_t value) noexcept;  // going on from the CRC of what came before (Go's crc32.Update)

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
uint32_t value() const noexcept;               // the CRC; update may go on
array<byte, 4> digest() const noexcept;   // the CRC, the most significant byte first
void reset() noexcept;

static uint32_t of(/* bytes or text */) noexcept;
static constexpr uint32_t combine(uint32_t first, uint32_t second, uint64_t second_length) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

`crc32c` has the same members.

```cpp
using namespace sgcl;

hash::crc32::of("123456789");                  // 0xCBF43926, the catalogue's check
hash::crc32c::of("123456789");                 // 0xE3069283

hash::crc32 h;
h.update(header);
h.update(body);
uint32_t sum = h.value();

// A CRC saved with the data, extended later with what was appended
auto more = hash::crc32::resume(saved_crc);
more.update(appended);
```

## combine

`combine(first, second, n)` is the CRC of A followed by B, given the CRC of A, the CRC of B and `n`, the length of B in bytes: zlib's `crc32_combine`, for both types. Moving a CRC past `n` zero bytes is a multiplication by x^(8n) modulo the polynomial, and the powers x^(8·2^k) are a table the compiler computes, so a combine is one multiplication for every set bit of `n`, for any length up to 2^64 − 1. It is `constexpr`. The [module's page](README.md#two-pieces-one-checksum) has it hashing a buffer on four tasks.

## In a file format

`digest()` is big-endian, as Go's `Sum` writes it. gzip and zip store the CRC little-endian, PNG big-endian: a format writes `value()` in its own order rather than taking `digest()` for the bytes it needs.

## Paths

On arm64 an input of 128 bytes or more is folded 64 bytes at a time by carry-less multiplication (PMULL), in four independent lanes, and the last 16 bytes of the fold and anything shorter go through the CRC-32 instructions (`crc32x`, `crc32cx`). Folding wins even where there is an instruction: the instruction takes eight bytes and needs the register the last one left, and four lanes of sixteen do not wait for each other. Elsewhere the path is slicing by eight over tables the compiler builds from the polynomial (8 KB each), which x86 takes until its own instructions come. The two paths give the same result for every input; the tests hold them against each other and against Go.

## Example

```cpp
#include "sgcl/hash/crc32.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

using namespace sgcl;

int main() {
    io::stdout.write(txt::format("{:08x}\n", hash::crc32::of("123456789")));    // the catalogue's check
    io::stdout.write(txt::format("{:08x}\n", hash::crc32c::of("123456789")));

    // a file's CRC kept beside it, extended when more is appended
    uint32_t saved = hash::crc32::of("first line\n");
    auto more = hash::crc32::resume(saved);
    more.update("second line\n");
    io::stdout.write(txt::format("{}\n", more.value() == hash::crc32::of("first line\nsecond line\n")));

    // the same from the two pieces' CRCs and the second one's length
    uint32_t joined = hash::crc32::combine(saved, hash::crc32::of("second line\n"), 12);
    io::stdout.write(txt::format("{}\n", joined == more.value()));
}
```

Output:

```text
cbf43926
e3069283
true
true
```

## See also

[The module](README.md); [`crc64`](crc64.md); [`adler32`](adler32.md), zlib's other checksum; [`mixin::hasher`](hasher.md).
