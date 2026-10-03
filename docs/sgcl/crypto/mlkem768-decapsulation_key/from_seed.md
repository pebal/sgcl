[sgcl](../../README.md) › [crypto](../README.md) › [mlkem768](../mlkem.md) › [decapsulation_key](README.md)

# sgcl::crypto::mlkem768::decapsulation_key::from_seed

```cpp
static expected<decapsulation_key, error> from_seed(const slice<const byte>& seed) noexcept;
```

The key of its seed d‖z (FIPS 203 §7.1: d the 32 bytes of the key generation, z the 32 of the implicit rejection),
Go's `NewDecapsulationKey768`: the form [seed](seed.md) gives and a key is stored in. The same seed always gives the
same key, in every implementation of FIPS 203. `mlkem512::decapsulation_key::from_seed` and
`mlkem1024::decapsulation_key::from_seed` take the same 64 bytes and make keys of their sets.

## Parameters

| Parameter | Description |
|---|---|
| `seed` | the seed d‖z, 64 bytes (`seed_size`) |

## Return value

The key, or a [crypto::error](../error/README.md) with `errc::invalid_key` for a seed of another length.

## Complexity

Constant: the key generation of FIPS 203, the matrix sampled once.

## Exceptions

None.

## Notes

The seed is a secret: a seed a program keeps belongs in a [secret](../secret/README.md) or a
[secret_bytes](../secret_bytes/README.md) ([read_secret](../read_secret.md) reads a file into one), never in a managed
`vector` or `string`, which the collector frees without zeroing.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a seed of the tests, d = 00..1f and z = 20..3f
    auto key = crypto::mlkem768::decapsulation_key::from_seed(encoding::hex::decode(
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
        "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"));
    auto published = key->encapsulation_key().bytes();
    println("SHA-256 of the encapsulation key {}",
            encoding::hex::encode(crypto::sha256::of(published)).substr(0, 16));

    auto short_seed = crypto::mlkem768::decapsulation_key::from_seed(vector<byte>(32));
    println("{}", short_seed.error().message());
}
```

Output:

```text
SHA-256 of the encapsulation key 0b7934c83125c788
sgcl::crypto::mlkem768: a seed is 64 bytes
```

## See also

- [seed](seed.md): the seed of a key
- [generate](generate.md): a key of a fresh seed
- [sgcl::crypto::mlkem768::decapsulation_key](README.md)
