[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::sign

```cpp
static string sign(const slice<const byte>& payload, const jwk& key);
static string sign(const slice<const byte>& payload, const jwk& key, const sign_options& o);
```

The compact serialization of the payload signed by the key (RFC 7515 §7.1): the protected header, the payload and
the signature, each in base64url, joined by dots. The header holds `alg` first, the key's `kid` when it has one, and
the members of `o.header`. The algorithm is `o.alg`, else the key's `alg`, else its kind's (oct HS256, RSA RS256, P-256
ES256, P-384 ES384, P-521 ES512, Ed25519 EdDSA). ECDSA's nonce is RFC 6979's hedged with random bytes, as the module's ECDSA signs.

## Parameters

| Parameter | Description |
|---|---|
| `payload` | the payload, bytes or text |
| `key` | a private key, or an oct key |
| `o` | the algorithm and the header ([sign_options](../jose-sign_options.md)) |

## Return value

The JWS.

## Complexity

Linear in the length of the payload, and the signature.

## Exceptions

`std::invalid_argument` when the key cannot sign with the algorithm: a public key, a key of another kind or curve, an
algorithm that is not a signature's, an HMAC key shorter than the digest or an RSA key under 2048 bits (RFC 7518), a
key whose `alg`, `use` or `key_ops` forbid it, a header that is not a JSON object.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::symmetric("a secret of thirty-two bytes ...", {.kid = "s1"});
    println("{}", crypto::jose::jws::sign("hello", key));
    println("{}", crypto::jose::jws::sign("hello", key, {.alg = crypto::jose::algorithm::hs256}));
}
```

Output:

```text
eyJhbGciOiJIUzI1NiIsImtpZCI6InMxIn0.aGVsbG8.kbj019AfBOqx4HFulzvC9yCe_m5Mstc-rJP4TQYCn0g
eyJhbGciOiJIUzI1NiIsImtpZCI6InMxIn0.aGVsbG8.kbj019AfBOqx4HFulzvC9yCe_m5Mstc-rJP4TQYCn0g
```

## See also

- [sign_json](sign_json.md): the JSON serialization
- [verify](verify.md)
- [sgcl::crypto::jose::jws](README.md)
