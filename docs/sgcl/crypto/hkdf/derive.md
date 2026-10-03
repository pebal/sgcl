[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](README.md)

# sgcl::crypto::hkdf\<H\>::derive

```cpp
static secret_bytes derive(const slice<const byte>& salt, const slice<const byte>& ikm,
                           const slice<const byte>& info, size_t n);
```

[extract](extract.md) and [expand](expand.md) in one: `n` bytes from the input keying material `ikm` under `salt`,
bound to `info`, what Go's `hkdf.Key` gives. The PRK between the two steps is zeroed before the call returns; `n` is
checked before anything is computed. The output is a [secret_bytes](../secret_bytes/README.md), as `expand` gives it.

## Parameters

| Parameter | Description |
|---|---|
| `salt` | a value both sides know, not a secret; may be empty |
| `ikm` | the input keying material |
| `info` | what the output is for; may be empty |
| `n` | the number of bytes, at most `max_size` |

## Return value

The `n` bytes.

## Complexity

Linear in the lengths of `salt` and `ikm`, and in `n`.

## Exceptions

`std::invalid_argument` when `n` is greater than `max_size`, 255 blocks of the digest.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869, test case 3: no salt, no info
    vector<byte> ikm(22, byte(0x0b));
    println(encoding::hex::encode(crypto::hkdf_sha256::derive("", ikm, "", 42)));
}
```

Output:

```text
8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8
```

## See also

- [derive_to](derive_to.md): into a buffer of the caller's
- [extract](extract.md), [expand](expand.md): the two steps apart, for several keys from one PRK
- [sgcl::crypto::hkdf\<H\>](README.md)
