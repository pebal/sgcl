# sgcl::hash::crc64, sgcl::hash::crc64_iso

```cpp
#include "sgcl/hash/crc64.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class crc64;       // CRC-64/XZ: xz, 7z (Go's crc64.ECMA)
    class crc64_iso;   // CRC-64/GO-ISO (Go's crc64.ISO)
}
```

The two 64-bit CRCs, with the members of [`crc32`](crc32.md) and a `uint64_t` where that has a `uint32_t`: `resume(v)`, going on from a saved CRC, `value()`, `digest()` (eight bytes, the most significant first), `reset()`, `of`, `combine` and `copy_from`.

## Members

```cpp
static constexpr size_t digest_size = 8;
static constexpr size_t block_size = 1;

crc64() noexcept;                              // the initial state: the CRC of nothing is 0
static crc64 resume(uint64_t value) noexcept;  // going on from the CRC of what came before

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
uint64_t value() const noexcept;               // the CRC; update may go on
array<byte, 8> digest() const noexcept;   // the CRC, the most significant byte first
void reset() noexcept;

static uint64_t of(/* bytes or text */) noexcept;
static constexpr uint64_t combine(uint64_t first, uint64_t second, uint64_t second_length) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
static expected<uint64_t, io::error> of_file(const string& path);  static async::task<expected<uint64_t, io::error>> async_of_file(const string& path);   // of() of the whole file, through copy_from
```

`crc64_iso` has the same members.

## The names

`crc64` is what xz, 7z and Go compute, and Go calls it `crc64.ECMA`. The name here is the one the [CRC catalogue](https://reveng.sourceforge.io/crc-catalogue/) gives it, CRC-64/XZ, because the catalogue has a CRC-64/ECMA-182 as well: the same polynomial, neither reflected nor inverted, and another result for the same bytes (`6C40DF5F0B497347` for the check string). A `crc64_ecma` would send whoever checks the catalogue to the wrong one. `crc64_iso` is Go's `crc64.ISO`, the polynomial of ISO 3309, there to agree with data Go wrote; it is a weak code for anything new.

## Paths

There is no CRC-64 instruction. On arm64 an input of 128 bytes or more is folded by carry-less multiplication, as for the 32-bit CRCs, with the last 16 bytes of the fold and anything shorter going through slicing by eight; elsewhere slicing by eight does all of it, over tables of 16 KB per polynomial that the compiler builds.

## Example

```cpp
#include "sgcl/hash/hash.h"
#include "sgcl/io/io.h"

using namespace sgcl;

int main() {
    println("{:016x}", hash::crc64::of("123456789"));  // CRC-64/XZ's check
    println("{:016x}", hash::crc64_iso::of("123456789"));
    hash::crc64 h;
    h.update("1234");
    h.update("56789");
    println("{:016x}", h.value());
}
```

Output:

```text
995dc9bbdf1939fa
b90956c775a41001
995dc9bbdf1939fa
```

## See also

[The module](README.md); [`crc32`](crc32.md), with the same members; [`mixin::hasher`](hasher.md).
