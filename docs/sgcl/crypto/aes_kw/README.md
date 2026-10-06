[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::aes_kw

```cpp
#include "sgcl/crypto/kw.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class aes_kw;
}
```

`sgcl::crypto::aes_kw` is AES key wrap (RFC 3394, SP 800-38F's KW) and key wrap with padding (RFC 5649, KWP) under a
key-encryption key: a key encrypted with an integrity check, deterministically, for keys and nothing else. It is
JOSE's `A128KW` and `A256KW`, CMS's key-encryption-key recipients, PKCS #11's `CKM_AES_KEY_WRAP`, the key stores of
cloud KMSs. [wrap](wrap.md) takes a key of 16 bytes or more and a multiple of 8 and gives 8 bytes more;
[wrap_padded](wrap_padded.md) takes any length and pads it; [unwrap](unwrap.md) and [unwrap_padded](unwrap_padded.md)
give the key back as a [secret_bytes](../secret_bytes/README.md), or `errc::authentication` when the check does not
come out — another key-encryption key, or a changed byte.

Six passes over the key's 64-bit halves, each step one AES encryption of a check register and one half with the
step's number XORed into the register; unwrapping runs them back and checks that the register came out as it started.
Go's standard library has no key wrap; OpenSSL's `EVP_aes_*_wrap` and `EVP_aes_*_wrap_pad` and Wycheproof's vectors are
what the tests hold it to.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The key-encryption key lives in the object**, as an [aes](../aes/README.md) key does: move-only, zeroed by the
  destructor and by a move out of it, a copy by [clone](clone.md). A key unwrapped is a `secret_bytes`, zeroed when
  it goes.
- **A key from data is a value, a key in the program a contract**: [from_key](from_key.md) gives
  `errc::invalid_key` for a key-encryption key of the wrong length; a key to wrap of a length the method does not
  take is `invalid_argument`, thrown.
- **The check is one answer**: every way a wrapped key can fail — RFC 3394's register, RFC 5649's constant, its
  length and its zero padding — is the same `errc::authentication`, all checked with no early exit; a length that
  cannot be a wrapped key is `errc::malformed`.
- **Deterministic**: one key wraps to the same bytes under one key-encryption key, which is what makes it a key wrap
  and not a cipher for data.
- **Thread-safe for reading**: every method is `const`; one object may wrap and unwrap from several threads.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](aes_kw.md) | sets up the key-encryption key, or takes another object's over |
| `(destructor)` | overwrites the key schedules with zeros |
| [operator=](operator_assign.md) | takes another object's key over |
| [from_key](from_key.md) | sets up a key-encryption key that came with data, into an `expected` (static) |
| [clone](clone.md) | a copy of the key, made on purpose |
| [key_size](key_size.md) | 16, 24 or 32; 0 after a move |

#### Key wrap

| Function | Description |
|---|---|
| [wrap](wrap.md) | a key of 8 n bytes wrapped (RFC 3394) |
| [unwrap](unwrap.md) | a key wrapped by `wrap`, checked and given back |
| [wrap_padded](wrap_padded.md) | a key of any length wrapped with its length (RFC 5649) |
| [unwrap_padded](unwrap_padded.md) | a key wrapped by `wrap_padded`, checked and given back |

## Complexity

Linear in the key's length: six AES operations for each 8 bytes, through the processor's AES instructions where it
has them and the bitsliced AES elsewhere, in constant time.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 3394 §4.1: 128 bits of key under a 128-bit key-encryption key
    vector<byte> kek = encoding::hex::decode("000102030405060708090a0b0c0d0e0f");
    vector<byte> key = encoding::hex::decode("00112233445566778899aabbccddeeff");
    crypto::aes_kw wrapper(kek);
    vector<byte> wrapped = wrapper.wrap(key);
    println(encoding::hex::encode(wrapped));

    crypto::secret_bytes back = wrapper.unwrap(wrapped).value();
    println(encoding::hex::encode(back));
}
```

Output:

```text
1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5
00112233445566778899aabbccddeeff
```

## See also

- [aes](../aes/README.md): the block cipher alone
- [secret_bytes](../secret_bytes/README.md): where an unwrapped key goes
- [The module](../README.md)
