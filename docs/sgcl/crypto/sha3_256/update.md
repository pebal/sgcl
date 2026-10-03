[sgcl](../../README.md) › [crypto](../README.md) › [sha3_256](README.md)

# sgcl::crypto::sha3_256::update

```cpp
void update(const slice<const byte>& data) noexcept;
```

Absorbs the bytes of `data` into the sponge, after everything absorbed before: a message fed in pieces of any length
has the digest of the whole. The slice takes what bytes come in: a `vector<byte>`, an `array<byte, N>`, a
`secret_bytes`, a slice of them. The bytes are XORed into the state, and every time a block of the rate is full the
state is permuted.

The text forms — a `string`, a text slice, a literal, a C string, a `std::string_view` — and a `std::span` of bytes
are the mixin's ([hash::mixin::hasher](../../hash/mixin/hasher/README.md)), each hashing the UTF-8 bytes where they lie.

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
    crypto::sha3_256 h;
    h.update("abcdbcdecdefdefgefghfghighij");
    h.update("hijkijkljklmklmnlmnomnopnopq");
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376
```

## See also

- [value](value.md): the digest of what was hashed in
- [hash::mixin::hasher](../../hash/mixin/hasher/README.md): the text forms, `copy_from` for a stream
- [sgcl::crypto::sha3_256](README.md)
