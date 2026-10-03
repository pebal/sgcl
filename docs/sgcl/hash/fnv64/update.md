[sgcl](../../README.md) › [hash](../README.md) › [fnv64](README.md)

# sgcl::hash::fnv64::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Hashes `data` after the bytes hashed before: for every byte the hasher multiplies first, then XORs the byte, the
prime `0x100000001b3`. The slice takes the forms bytes come in — a `vector<byte>`, a `std::vector<std::byte>`, an
array of `uint8_t`, the bytes a read of io filled, a part of any of them — as a `slice<const byte>` takes them. A
text, the digest of another hasher and a `std::span` of bytes go in through the overloads of
[mixin::hasher::update](../mixin/hasher/update.md), which hand their bytes to this one.

It never fails: Go's `h.Write(p)`, whose error is always nil.

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
    hash::fnv64 h;
    for (int i : range(10)) {
        h.update(data.as_slice(i * 100, 100));
    }
    println("{}", h.value() == hash::fnv64::of(data));
    println("{:016x}", h.value());
}
```

Output:

```text
true
a2ed10da2ff3a0d5
```

## See also

- [mixin::hasher::update](../mixin/hasher/update.md): a text, a digest, a std::span of bytes
- [of](../mixin/hasher/of.md): the same in one call
- [sgcl::hash::fnv64](README.md)
