[sgcl](../../README.md) › [hash](../README.md) › [xxh64](README.md)

# sgcl::hash::xxh64::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Hashes `data` after the bytes hashed before: the hasher takes whole stripes of 32 bytes into its lanes and keeps
the rest until a stripe is full, so that a hash in pieces is the hash in one call. The slice takes the forms bytes
come in — a `vector<byte>`, a `std::vector<std::byte>`, an array of `uint8_t`, the bytes a read of io filled, a part
of any of them — as a `slice<const byte>` takes them. A text, the digest of another hasher and a `std::span` of
bytes go in through the overloads of [mixin::hasher::update](../mixin/hasher/update.md), which hand their bytes to
this one.

It never fails.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to hash |

## Return value

None.

## Complexity

Linear in `data.size()`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> data(1000);
    for (int i : range(1000)) {
        data[i] = byte(i);
    }
    hash::xxh64 h;
    for (int i : range(10)) {
        h.update(data.as_slice(i * 100, 100));
    }
    println("{}", h.value() == hash::xxh64::of(data));
    println("{:016x}", h.value());
}
```

Output:

```text
true
6ef436b00eba4078
```

## See also

- [mixin::hasher::update](../mixin/hasher/update.md): a text, a digest, a std::span of bytes
- [of](../mixin/hasher/of.md): the same in one call
- [sgcl::hash::xxh64](README.md)
