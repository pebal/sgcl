[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::for_derive_key

```cpp
static blake3 for_derive_key(const slice<const byte>& context) noexcept;
```

A hasher in the derive_key mode under `context`: every `update` is key material, and the output — [value](value.md),
[value_to](value_to.md) — the key. It is [derive_key](derive_key.md) for key material that comes in pieces. The hasher
holds the context's key, and zeroes it in its destructor.

## Parameters

| Parameter | Description |
|---|---|
| `context` | the context string, bytes or text: fixed in the program, unique to the application and the purpose |

## Return value

The hasher.

## Complexity

Linear in the length of `context`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::blake3 kdf = crypto::blake3::for_derive_key("example.com 2026-10-05 session tokens v1");
    kdf.update("input key ");
    kdf.update("material");
    crypto::secret_bytes key(16);
    kdf.value_to(key);
    println(encoding::hex::encode(key));
}
```

Output:

```text
884481c60744026cf112569573a5acaf
```

## See also

- [derive_key](derive_key.md): the same in one call
- [sgcl::crypto::blake3](README.md)
