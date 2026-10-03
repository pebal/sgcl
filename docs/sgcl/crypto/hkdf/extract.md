[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](README.md)

# sgcl::crypto::hkdf\<H\>::extract

```cpp
static prk extract(const slice<const byte>& salt, const slice<const byte>& ikm) noexcept;
```

The pseudorandom key of the input keying material `ikm` under `salt`: `HMAC(salt, ikm)`, the first step of RFC 5869,
which concentrates the entropy of the input into `H::digest_size` bytes. An empty salt is the digest's size of zeros
(RFC 5869 §2.2), which HMAC's padding of the key makes the same thing. The tag the PRK is copied from is zeroed before
the call returns.

## Parameters

| Parameter | Description |
|---|---|
| `salt` | a value both sides know, not a secret; may be empty |
| `ikm` | the input keying material: the secret of a key exchange, a master key |

## Return value

The PRK, an [hkdf\<H\>::prk](../hkdf-prk/README.md) that zeroes itself when it dies.

## Complexity

Linear in the lengths of `salt` and `ikm`: one HMAC.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 5869, test cases 1 and 3
    vector<byte> ikm(22, byte(0x0b));
    auto salt = encoding::hex::decode("000102030405060708090a0b0c");
    auto master = crypto::hkdf_sha256::extract(salt, ikm);
    println(encoding::hex::encode(master.bytes()));
    println(encoding::hex::encode(crypto::hkdf_sha256::extract("", ikm).bytes()));
}
```

Output:

```text
077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5
19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04
```

## See also

- [expand](expand.md): the second step
- [derive](derive.md): both steps in one
- [sgcl::crypto::hkdf\<H\>](README.md)
