[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Hashes the bytes of `data` in, after everything hashed before: an input fed in pieces of any length has the output of
the whole. The whole chunks of `data` that more input follows are hashed straight from it, four at a time, and their
subtrees joined; the chunk at the end waits in the hasher, since the last chunk and the last two subtrees are joined
otherwise when they are the root, and only the next call or the output tells which they are.

The text forms — a `string`, a text slice, a literal, a C string, a `std::string_view` — and a `std::span` of bytes
are the mixin's ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)).

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
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::blake3 h;
    h.update("hello ");
    h.update("world");
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24
```

## See also

- [value](value.md): the hash of what was hashed in
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the text forms, `copy_from` for a stream
- [sgcl::crypto::blake3](README.md)
