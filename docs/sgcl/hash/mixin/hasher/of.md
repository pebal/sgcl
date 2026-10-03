[sgcl](../../../README.md) › [hash](../../README.md) › mixin › [hasher](../hasher.md)

# sgcl::hash::mixin::hasher\<Derived\>::of

```cpp
template<class... Args>
requires (sizeof...(Args) == 0 ? std::default_initializable<Derived>
                               : requires(const Args&... args) {
                                     Derived::_of(std::declval<const slice<const byte>&>(), args...);
                                 })
static auto of(const slice<const byte>& data, const Args&... args) noexcept;                             // (1)
template<class Data, class... Args>
static auto of(const Data& data, const Args&... args) noexcept;                                          // (2)
```

The hash of `data` in one call: `crc32::of(data)` is `crc32 h; h.update(data); return h.value();`, the value in the
type `value()` has.

1. Bytes: a `slice<const byte>` or anything one is made from, a `vector<byte>`, a `std::vector<std::byte>`, an
   array of `uint8_t`.
2. The other forms [update](update.md) takes, each as it takes them: a [string](../../../core/string.md), a slice
   of characters, a literal or an array of `char` (up to its first NUL), a C string, a `std::string_view` (and a
   `std::string` through it), a digest, a `std::span` of bytes. Takes part only for those forms, and only where (1)
   takes `args`.

- (1–2) With arguments after the data, a seed or a key, for a class whose hash takes one and which declares a
  one-shot of its own ([the rules](../hasher.md#rules)): `xxh3_64::of(data, seed)`, `maphash::of(data, seed)`,
  `siphash::of(data, key)`. Such a class hashes in one call with no hasher made. Without arguments `of` exists only
  for a class made without them, so `siphash::of(data)` does not compile; and only such a class takes an argument,
  so a CRC does not go on from a value through `of`: `crc32::of(data, v)` does not compile, and
  [crc32::resume](../../crc32/resume.md) is what goes on from a value.

It is the only one-shot form: no free function per algorithm, as Go has `crc32.ChecksumIEEE` and
`adler32.Checksum`, and no template over the algorithm.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes or the text to hash |
| `args` | the seed or the key of the class, after the data |

## Return value

The hash, in the type of the class's `value()`: `uint32_t`, `uint64_t` or `array<byte, 16>`.

## Complexity

Linear in the size of `data`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{:08x}", hash::crc32::of("123456789"));
    vector<byte> bytes = {byte('a'), byte('b'), byte('c')};
    println("{:08x} {:016x}", hash::adler32::of(bytes), hash::fnv64a::of(bytes));
    println("{}", encoding::hex::encode(hash::xxh3_128::of("abc")));

    // a seed or a key after the data
    println("{:016x}", hash::xxh3_64::of("abc", 42));
    array<byte, 16> key = {};
    println("{:016x}", hash::siphash::of("abc", key));
}
```

Output:

```text
cbf43926
024d0127 e71fa2190541574b
06b05ab6733a618578af5f94892f3950
d8438def21bbdcc3
3fc884964770eede
```

## See also

- [update](update.md): the same forms, in pieces
- [of_file](of_file.md): the hash of a whole file
- [sgcl::hash::mixin::hasher\<Derived\>](../hasher.md)
