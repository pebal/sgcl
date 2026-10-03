[sgcl](../../README.md) › [crypto](../README.md) › [sha1](../sha1.md)

# sgcl::crypto::sha1::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Hashes the bytes of `data` in, after everything hashed before: a message fed in pieces of any length has the digest of
the whole. The slice takes what bytes come in: a `vector<byte>`, an `array<byte, N>`, a `secret_bytes`, a slice of
them. A whole block is compressed at once, the rest waits in the hasher's buffer for the next call or for `value()`.

The text forms — a `string`, a text slice, a literal, a C string, a `std::string_view` — and a `std::span` of bytes
are the mixin's ([hash::mixin::hasher](../../hash/mixin/hasher.md)), each hashing the UTF-8 bytes where they lie.

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
    // FIPS 180-4's example of two blocks, fed in two pieces
    crypto::sha1 h;
    h.update("abcdbcdecdefdefgefghfghighij");
    h.update("hijkijkljklmklmnlmnomnopnopq");
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
84983e441c3bd26ebaae4aa1f95129e5e54670f1
```

## See also

- [value](value.md): the digest of what was hashed in
- [hash::mixin::hasher](../../hash/mixin/hasher.md): the text forms, `copy_from` for a stream
- [sgcl::crypto::sha1](../sha1.md)
