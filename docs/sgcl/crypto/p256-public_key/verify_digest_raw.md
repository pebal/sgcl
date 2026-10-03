[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](../p256-public_key.md)

# sgcl::crypto::p256::public_key::verify_digest_raw

```cpp
[[nodiscard]] bool verify_digest_raw(const slice<const byte>& digest,
                                     const slice<const byte>& signature) const noexcept;
```

Checks that `signature`, the fixed-size r ‖ s of IEEE P1363 (what
[sign_digest_raw](../p256-private_key/sign_digest_raw.md) gives; the form of JWS, WebAuthn, PKCS #11 and COSE), signs
`digest` under this key, Go's `ecdsa.Verify` over r and s. The signature is `signature_size` bytes, 64 on P-256 and 96
on P-384, r and s big-endian; another length is false. The digest and the check are as for
[verify_digest](verify_digest.md): any length of one byte or more, r and s in [1, n − 1], x(u₁G + u₂Q) mod n equal to
r.

## Parameters

| Parameter | Description |
|---|---|
| `digest` | the digest the signer signed, of the hash the protocol names |
| `signature` | the signature r ‖ s, `signature_size` bytes |

## Return value

`true` when the signature is valid for the digest under this key; `false` for anything else: an empty digest, a
signature of another length, r or s out of range, a signature that does not verify.

## Complexity

Constant: two multiplications of points, by public scalars.

## Exceptions

None. A signature that cannot be one is `false`, never an exception.

## Notes

The function is `[[nodiscard]]`: a check whose result is dropped was never made. A signature in DER is not r ‖ s and
is false here.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the public key of RFC 6979, A.2.5, and its signature of SHA-256("test"), r || s
    auto key = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"));
    auto signature = encoding::hex::decode(
        "f1abb023518351cd71d881567b1ea663ed3efcf6c5132b354f28d3b0b7d38367"
        "019f4113742a2b14bd25926b49c649155f267e60d3814b4c0cc84250e46f0083");
    auto digest = crypto::sha256::of("test");

    println("{}", key->verify_digest_raw(digest, signature));
    println("{}", key->verify_digest(digest, signature));  // not DER

    (*signature)[63] ^= byte(1);
    println("{}", key->verify_digest_raw(digest, signature));
}
```

Output:

```text
true
false
false
```

## See also

- [verify_digest](verify_digest.md): a signature in DER
- [p256::private_key::sign_digest_raw](../p256-private_key/sign_digest_raw.md): the signature it checks
- [ECDSA](../ecdsa.md): the digest, the encodings
- [sgcl::crypto::p256::public_key](../p256-public_key.md)
