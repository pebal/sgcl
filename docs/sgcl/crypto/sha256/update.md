[sgcl](../../README.md) › [crypto](../README.md) › [sha256](../sha256.md)

# sgcl::crypto::sha256::update

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
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // FIPS 180-4's long example: a million times "a"
    crypto::sha256 h;
    string thousand(1000, 'a');
    for (int i : range(1000)) {
        h.update(thousand);
    }
    println(encoding::hex::encode(h.value()));

    // bytes and text alike
    crypto::sha256 g;
    g.update(vector<byte>{byte('a'), byte('b')});
    g.update("c");
    println(encoding::hex::encode(g.value()));
}
```

Output:

```text
cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

## See also

- [value](value.md): the digest of what was hashed in
- [hash::mixin::hasher](../../hash/mixin/hasher.md): the text forms, `copy_from` for a stream
- [sgcl::crypto::sha256](../sha256.md)
