[sgcl](../../README.md) › [crypto](../README.md) › [sha3_256](README.md)

# sgcl::crypto::sha3_256::value

```cpp
array<byte, 32> value() const noexcept;   // sha3_256
array<byte, 28> value() const noexcept;   // sha3_224
array<byte, 48> value() const noexcept;   // sha3_384
array<byte, 64> value() const noexcept;   // sha3_512
```

The digest of everything hashed in so far. The padding (the domain bits `01` of SHA-3 and the pad of FIPS 202) and
the squeeze are done on a copy of the state, so the hasher goes on: more `update` after `value()` gives the digest of
the longer message.

## Parameters

None.

## Return value

The digest: 32 bytes for `sha3_256`, 28 for `sha3_224`, 48 for `sha3_384`, 64 for `sha3_512`.

## Complexity

Constant: one permutation of the last block.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::sha3_256 h;
    h.update("a");
    println(encoding::hex::encode(h.value()));
    h.update("bc");  // goes on after "a"
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
80084bf2fba02475726feb2cab2d8215eab14bc6bdd8bfb2c8151257032ecd8b
3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
```

## See also

- [digest](digest.md): the same bytes, under the name every hasher has
- [update](update.md): hashes bytes in
- [sgcl::crypto::sha3_256](README.md)
