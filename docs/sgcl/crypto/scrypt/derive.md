[sgcl](../../README.md) › [crypto](../README.md) › [scrypt](README.md)

# sgcl::crypto::scrypt::derive

```cpp
static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n = 32);    // (1)
static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n,
                           const options& o);                                                                   // (2)
```

`n` bytes derived from `password` and `salt` with the costs of `o`, Go's `scrypt.Key`. The salt is not secret but
should be unique to each password, 16 random bytes ([random::bytes](../random/bytes.md)) as a rule.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password, bytes or text, of any length |
| `salt` | the salt, bytes or text, of any length |
| `n` | the bytes of key, up to (2^32 - 1) · 32; 32 by default |
| `o` | `N`, `r` and `p` ([options](../scrypt-options.md); (1) the defaults) |

## Return value

The key, `n` bytes, as a [secret_bytes](../secret_bytes/README.md).

## Complexity

Linear in `N · r · p`.

## Exceptions

`std::invalid_argument` when `o.cost` is not a power of two above 1, `o.block_size` or `o.parallelism` is 0, their
product reaches 2^30, the memory is past the address space, or `n` is past (2^32 - 1) · 32.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::scrypt::derive("password", "NaCl", 32,
                                                      {.cost = 1024, .block_size = 8, .parallelism = 16});
    println(encoding::hex::encode(key));
}
```

Output:

```text
fdbabe1c9d3472007856e7190d01e9fe7c6ad7cbc8237830e77376634b373162
```

## See also

- [derive_to](derive_to.md): into the program's buffer
- [sgcl::crypto::scrypt](README.md)
