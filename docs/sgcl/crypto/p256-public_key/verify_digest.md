[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](README.md)

# sgcl::crypto::p256::public_key::verify_digest

```cpp
[[nodiscard]] bool verify_digest(const slice<const byte>& digest,
                                 const slice<const byte>& signature) const noexcept;
```

Checks that `signature`, an ECDSA-Sig-Value in DER (what [sign_digest](../p256-private_key/sign_digest.md) gives,
what X.509 and TLS carry), signs `digest` under this key, Go's `ecdsa.VerifyASN1`. The digest is any length of one
byte or more, its leftmost bits used as FIPS 186-5 §6.4 has it ([ECDSA](../ecdsa.md#rules)).

The check is strict: the DER must be DER (lengths and integers in their shortest form, no negative number, nothing
after the sequence), r and s must be in [1, n − 1], and x(u₁G + u₂Q) mod n must equal r. A signature r ‖ s is not DER
and is false here: [verify_digest_raw](verify_digest_raw.md) takes that form. `p384::public_key::verify_digest`
checks P-384's signatures, at most 104 bytes of DER.

## Parameters

| Parameter | Description |
|---|---|
| `digest` | the digest the signer signed, of the hash the protocol names |
| `signature` | the signature, an ECDSA-Sig-Value in DER |

## Return value

`true` when the signature is valid for the digest under this key; `false` for anything else: an empty digest, a
signature that is not strict DER, r or s out of range, a signature that does not verify.

## Complexity

Constant: two multiplications of points, by public scalars.

## Exceptions

None. A signature that cannot be one is `false`, never an exception.

## Notes

The function is `[[nodiscard]]`: a check whose result is dropped was never made. Verification works on public data
and is not in constant time. As in every ECDSA, (r, n − s) verifies wherever (r, s) does
([ECDSA](../ecdsa.md#rules)).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the public key of RFC 6979, A.2.5, and its signature of SHA-256("sample") in DER
    auto key = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"));
    auto signature = encoding::hex::decode(
        "3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716"
        "022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8");
    auto digest = crypto::sha256::of("sample");

    println("{}", key->verify_digest(digest, signature));
    println("{}", key->verify_digest(crypto::sha256::of("test"), signature));
    println("{}", key->verify_digest(vector<byte>(), signature));

    signature->push_back(byte(0));  // a byte after the DER
    println("{}", key->verify_digest(digest, signature));
}
```

Output:

```text
true
false
false
false
```

## See also

- [verify_digest_raw](verify_digest_raw.md): a signature r ‖ s
- [p256::private_key::sign_digest](../p256-private_key/sign_digest.md): the signature it checks
- [ECDSA](../ecdsa.md): the digest, the encodings
- [sgcl::crypto::p256::public_key](README.md)
