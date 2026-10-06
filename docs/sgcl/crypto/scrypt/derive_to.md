[sgcl](../../README.md) › [crypto](../README.md) › [scrypt](README.md)

# sgcl::crypto::scrypt::derive_to

```cpp
static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt);    // (1)
static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt,
                      const options& o);                                                                            // (2)
```

[derive](derive.md) into the program's own buffer: `out.size()` bytes. The password is read whole before a byte of
`out` is written, so `out` may lie over it; the salt is read again at the end, so `out` must not lie over it.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the key goes: a `secret_bytes`, an `array`, the program's buffer |
| `password` | the password, bytes or text |
| `salt` | the salt, bytes or text |
| `o` | `N`, `r` and `p` ([options](../scrypt-options.md); (1) the defaults) |

## Return value

None.

## Complexity

As [derive](derive.md).

## Exceptions

`std::invalid_argument` when `out` overlaps `salt`, and in every case [derive](derive.md) throws it; nothing is
written then.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<byte, 32> key;
    crypto::scrypt::derive_to(key, "correct horse battery staple", "0123456789abcdef");
    println(encoding::hex::encode(key));
}
```

Output:

```text
f6b71517e0d9f2e53beeacf71ffbf6f7e9f683c73cefb00e0915d242f0bf7ecd
```

## See also

- [derive](derive.md): the key as a new secret_bytes
- [sgcl::crypto::scrypt](README.md)
