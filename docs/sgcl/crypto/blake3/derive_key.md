[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::derive_key

```cpp
static secret_bytes derive_key(const slice<const byte>& context, const slice<const byte>& key_material,
                               size_t n = 32) noexcept;
```

The derive_key mode of BLAKE3 in one call: `n` bytes of key from `key_material` under `context`. The context is
hashed first into a key of its own, under which the key material is hashed, so one key material gives unrelated keys
under two contexts. The context is a string fixed in the program, unique to the application and the purpose — a
name, a date, what the key is for — and never a secret or a variable; the key material is the secret: a shared secret
of a key agreement, a master key. Where a protocol names HKDF, [hkdf](../hkdf/README.md) is the one.

## Parameters

| Parameter | Description |
|---|---|
| `context` | the context string, bytes or text |
| `key_material` | the secret the key is derived from |
| `n` | the bytes of key, 32 by default; any number |

## Return value

The key, `n` bytes, as a [secret_bytes](../secret_bytes/README.md).

## Complexity

Linear in the lengths of `context` and `key_material`, and in `n`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::blake3::derive_key("example.com 2026-10-05 session tokens v1",
                                                          "input key material");
    println(encoding::hex::encode(key));
}
```

Output:

```text
884481c60744026cf112569573a5acaf5858e2726c7f4a5373f3f67f478ff2a0
```

## See also

- [for_derive_key](for_derive_key.md): the key material in pieces
- [hkdf](../hkdf/README.md): the key derivation of TLS
- [sgcl::crypto::blake3](README.md)
