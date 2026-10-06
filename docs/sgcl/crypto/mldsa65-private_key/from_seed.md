[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [private_key](README.md)

# sgcl::crypto::mldsa65::private_key::from_seed

```cpp
static expected<private_key, error> from_seed(const slice<const byte>& seed) noexcept;
```

The key of a seed ξ of 32 bytes (ML-DSA.KeyGen_internal, FIPS 204 Algorithm 6): the private key's form RFC 9881 recommends and Go's `mldsa.NewPrivateKey` takes, what [seed](seed.md) gives back. The same seed gives the same key here and in Go, byte for byte.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | 32 bytes: a [secret\<32\>](../secret/README.md) of [seed](seed.md), a [secret_bytes](../secret_bytes/README.md) of [read_secret](../read_secret.md), bytes |

## Return value

The key, or `errc::invalid_key` for a seed of another length.

## Complexity

Constant: one key generation.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a key of a fixed seed (a program makes its own with generate())
    auto key = crypto::mldsa65::private_key::from_seed(
        encoding::hex::decode("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")).value();
    println("{}", encoding::hex::encode(key.public_key().bytes()).substr(0, 16));
}
```

Output:

```text
48683d91978e31eb
```

## See also

- [seed](seed.md), [generate](generate.md)
- [sgcl::crypto::mldsa65::private_key](README.md)
