[sgcl](../../README.md) › [crypto](../README.md) › [argon2](README.md)

# sgcl::crypto::argon2::derive_to

```cpp
static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt);    // (1)
static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt,
                      const options& o);                                                                            // (2)
```

[derive](derive.md) into the program's own buffer: `out.size()` bytes, nothing allocated for the result. The
password, the salt and the optional inputs are hashed whole before the first byte of `out` is written, so `out` may
lie over any of them.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the key goes, at least 4 bytes: a `secret_bytes`, an `array`, the program's buffer |
| `password` | the password, bytes or text |
| `salt` | the salt, at least 8 bytes |
| `o` | the variant, the costs, the secret and the associated data ([options](../argon2-options.md); (1) the defaults) |

## Return value

None.

## Complexity

As [derive](derive.md).

## Exceptions

`std::invalid_argument` when `out` is shorter than 4 bytes, and in every case [derive](derive.md) throws it; nothing
is written then.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key(32);
    vector<byte> pepper(32, byte(7));
    crypto::argon2::derive_to(key, "password", "somesalt",
                              {.memory = 8192, .iterations = 1, .parallelism = 1, .secret = pepper});
    println(encoding::hex::encode(key));
}
```

Output:

```text
0d78eea9d82886c55ee534e6f88750a01e0b8f32e5f73d56b8c88567f44def09
```

## See also

- [derive](derive.md): the key as a new secret_bytes
- [sgcl::crypto::argon2](README.md)
