[sgcl](../../README.md) › [crypto](../README.md) › [argon2](README.md)

# sgcl::crypto::argon2::derive

```cpp
static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n = 32);    // (1)
static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n,
                           const options& o);                                                                   // (2)
```

`n` bytes derived from `password` and `salt` by Argon2 with the variant, the costs and the optional inputs of `o`:
the raw function, Go's `argon2.IDKey` and `argon2.Key`, for a key a format or a protocol derives with Argon2. The salt
is not secret but must be unique to each password: 16 random bytes ([random::bytes](../random/bytes.md)) as a rule, at
least 8. For storing a password to check it later, [generate](generate.md) is the simpler form.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password, bytes or text, of any length |
| `salt` | the salt, at least 8 bytes |
| `n` | the bytes of key, at least 4; 32 by default |
| `o` | the variant, the costs, the secret and the associated data ([options](../argon2-options.md); (1) the defaults) |

## Return value

The key, `n` bytes, as a [secret_bytes](../secret_bytes/README.md).

## Complexity

Linear in `o.memory` times `o.iterations`; the lanes side by side on the scheduler's workers.

## Exceptions

`std::invalid_argument` when the salt is shorter than 8 bytes, `n` is under 4, `o.iterations` is 0, `o.parallelism`
is 0 or past 2^24 - 1, or `o.memory` is less than 8 KiB a lane.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::argon2::derive("password", "somesalt");
    println(encoding::hex::encode(key));
}
```

Output:

```text
661fefbd6f29bcbc8f4646abc32a9d7a4645bb5c059537f8a5587f31adbecccd
```

## See also

- [derive_to](derive_to.md): into the program's buffer
- [generate](generate.md): a password hash for storage
- [sgcl::crypto::argon2](README.md)
