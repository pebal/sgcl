[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](../hkdf/README.md)

# sgcl::crypto::hkdf\<H\>::prk

```cpp
#include "sgcl/crypto/hkdf.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    template<class H>
    class hkdf {
    public:
        class prk;
    };
}
```

`sgcl::crypto::hkdf<H>::prk` is a pseudorandom key of RFC 5869: what [extract](../hkdf/extract.md) gives and
[expand](../hkdf/expand.md) takes, `H::digest_size` bytes held in the object itself. It is a secret, so it is treated as
one: it cannot be copied (`clone()` when a copy is meant), a move zeroes the object moved from, and the destructor
zeroes the bytes with stores the compiler cannot drop. Go's `hkdf.Extract` gives the PRK as a plain `[]byte` the
collector frees when it will; here the key does not outlive its object.

Only `extract` makes one; a PRK a protocol computed otherwise goes to `expand` as bytes.

## Rules

- **The bytes are in the object**, `H::digest_size` of them, no allocation: on a stack it is a stack array, and it
  lives anywhere a plain struct does. Keep it on the stack or in a `unique_ptr`: in a managed object it stays in
  memory until the cycle that finds it dead.
- **[bytes](bytes.md) gives them where they lie**, valid while the object lives, for a protocol that writes
  the PRK down or feeds it on (TLS 1.3 takes one secret from another).
- **Nothing fails**: every member is `noexcept`.

## Member objects

| Constant | Value | Description |
|---|---|---|
| `size` | `H::digest_size` | the bytes of the key, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](hkdf-prk.md) | the move constructor; no copy |
| `(destructor)` | zeroes the key |
| [operator=](operator_assign.md) | the move assignment; no copy |
| [clone](clone.md) | a second object with the same key |
| [bytes](bytes.md) | the key's bytes, where they lie in the object |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869, test case 1: one PRK, two keys for two uses
    vector<byte> ikm(22, byte(0x0b));
    auto salt = encoding::hex::decode("000102030405060708090a0b0c");
    crypto::hkdf_sha256::prk master = crypto::hkdf_sha256::extract(salt, ikm);
    println("{} {}", master.size, encoding::hex::encode(master.bytes()));
    println(encoding::hex::encode(crypto::hkdf_sha256::expand(master, "key", 16)));
    println(encoding::hex::encode(crypto::hkdf_sha256::expand(master, "iv", 12)));
}
```

Output:

```text
32 077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5
61222c33c932d703e6fe960de76a3883
8bf085089f628f4e24e8c5f7
```

## See also

- [extract](../hkdf/extract.md), [expand](../hkdf/expand.md): what makes it and what takes it
- [secure_zero](../secure_zero.md): how it is zeroed
- [sgcl::crypto::hkdf\<H\>](../hkdf/README.md)
