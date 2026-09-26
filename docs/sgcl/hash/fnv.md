# sgcl::hash::fnv32, fnv32a, fnv64, fnv64a, fnv128, fnv128a

```cpp
#include "sgcl/hash/fnv.h"   // or "sgcl/hash/hash.h"

namespace sgcl::hash {
    class fnv32;     // FNV-1, 32 bits       value(): uint32_t
    class fnv32a;    // FNV-1a, 32 bits      value(): uint32_t
    class fnv64;     // FNV-1, 64 bits       value(): uint64_t
    class fnv64a;    // FNV-1a, 64 bits      value(): uint64_t
    class fnv128;    // FNV-1, 128 bits      value(): array<byte, 16>
    class fnv128a;   // FNV-1a, 128 bits     value(): array<byte, 16>
}
```

The Fowler/Noll/Vo hashes ([draft-eastlake-fnv](https://datatracker.ietf.org/doc/draft-eastlake-fnv/)): a start value, the offset basis, and for every byte a multiplication by a prime and an XOR of the byte. FNV-1 multiplies first and FNV-1a XORs first. Nobody reaches for FNV today for speed — it is one multiplication a byte, each waiting for the last — but to agree with values somebody already computed, and then the variant and the width are not a choice. So all six are here, as Go has them, each a type with the [common shape](hasher.md).

## Members

```cpp
static constexpr size_t digest_size;       // 4, 8 or 16
static constexpr size_t block_size = 1;

fnv64a() noexcept;                                     // the offset basis
static fnv64a resume(uint64_t value) noexcept;         // going on from a value saved earlier (uint32_t for the 32-bit ones,
                                                       // array<byte, 16> for the 128-bit ones)

void update(const slice<const byte>& data) noexcept;    // and the text forms of the mixin
/* uint32_t, uint64_t, array<byte, 16> */ value() const noexcept;
array<byte, digest_size> digest() const noexcept;      // the most significant byte first
void reset() noexcept;
static /* the type of value() */ of(/* bytes or text */) noexcept;

expected<size_t, io::error> copy_from(const io::reader& r);  async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r);
```

```cpp
using namespace sgcl;

hash::fnv32a::of("foobar");     // 0xBF9CF968
hash::fnv64a::of("foobar");     // 0x85944171F73967E8
auto wide = hash::fnv128a::of("a");   // d2 28 cb 69 6f 1a 8c af 78 91 2b 70 4e 4a 89 64
```

The state of an FNV is its value, so the constructor from a value goes on where a saved hash left off, as [`crc32`](crc32.md) and [`adler32`](adler32.md) do (Go does it through `UnmarshalBinary`). There is no `combine`: the value after A says nothing that would let the hash of B be joined to it.

## 128 bits

The 128-bit result is sixteen bytes, the most significant first, so `value()` and `digest()` are the same thing: there is no 128-bit integer type to hand back, and the bytes are the form the result is written in anyway. The prime is 2^88 + 2^8 + 0x3b, so a multiplication by it is the state times 0x13b plus the state moved up 88 bits: two words, one small multiplication spread over both and a shift, no 128-bit arithmetic.

## Example

```cpp
#include "sgcl/encoding/hex.h"
#include "sgcl/hash/fnv.h"
#include "sgcl/io/os.h"
#include "sgcl/txt/format.h"

using namespace sgcl;

int main() {
    io::stdout.write(txt::format("{:08x}\n", hash::fnv32a::of("foobar")));
    io::stdout.write(txt::format("{:016x}\n", hash::fnv64a::of("foobar")));
    io::stdout.write(encoding::hex::encode(hash::fnv128a::of("a")) + "\n");

    // going on from a value saved earlier
    auto h = hash::fnv64a::resume(hash::fnv64a::of("foo"));
    h.update("bar");
    io::stdout.write(txt::format("{:016x}\n", h.value()));
}
```

Output:

```text
bf9cf968
85944171f73967e8
d228cb696f1a8caf78912b704e4a8964
85944171f73967e8
```

## See also

[The module](README.md); [`xxh3_64`](xxh3.md), a fast hash with fixed values for anything new; [`mixin::hasher`](hasher.md).
