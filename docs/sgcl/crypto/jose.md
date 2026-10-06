[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::jose

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    enum class algorithm : uint8_t;
    enum class encryption : uint8_t;
    enum class key_type : uint8_t;
    class jwk;
    class jwk_set;
    struct sign_options;
    class jws;
    struct encrypt_options;
    class jwe;
    class jwt;
}
```

`sgcl::crypto::jose` is JOSE, the JSON formats of signatures, encryption and keys that OAuth 2.0, OpenID Connect,
ACME and the APIs that sign their requests speak: a [JSON Web Signature](jose-jws/README.md) (RFC 7515), a
[JSON Web Encryption](jose-jwe/README.md) (RFC 7516), a [JSON Web Key](jose-jwk/README.md) and a
[key set](jose-jwk_set/README.md) (RFC 7517, with the thumbprint of RFC 7638), and the
[JSON Web Token](jose-jwt/README.md) (RFC 7519) made of a JWS whose payload is a set of claims. Go has no JOSE in
its standard library; this is what `github.com/go-jose/go-jose` and `golang-jwt/jwt` give a Go program. Every
algorithm under it is the module's own (HMAC, RSA, ECDSA, Ed25519, X25519, AES), the JSON is
[encoding::json](../encoding/json/README.md).

Written from RFC 7515, 7516, 7517, 7518, 7519, 7638 and 8037; tested against Project Wycheproof's JSON web vectors
(with RFC 7520's cookbook among them) and both ways against JOSE written by hand over Go's standard library: what
this module signs and encrypts Go verifies and decrypts, and the other way. **The implementation has not been through
an independent cryptographic audit.**

## Rules

- **The algorithm is the key's, never the token's.** A signature is verified only with an algorithm the key allows:
  the key's `alg` when it has one, else the algorithms of its kind (an oct key HS256, HS384 and HS512, an RSA key
  RS* and PS*, a P-256 key ES256, a P-384 key ES384, a P-521 key ES512, an Ed25519 key EdDSA). `none` does not exist. So a token
  cannot choose HS256 under an RSA public key's text, the confusion that broke early JWT libraries; a
  [key set](jose-jwk_set/README.md) that mixes symmetric keys with public ones is refused for verification, and so are
  two keys of one `kid`.
- **What a key is for.** A key whose `use` is not `sig` does not sign or verify, one whose `use` is not `enc` does
  not encrypt or decrypt; a `key_ops` list allows only what it names.
- **Sizes of RFC 7518.** An HMAC key at least as long as its digest, an RSA key of 2048 bits or more: refused when
  read for a verification or a decryption (`errc::invalid_key`), `std::invalid_argument` when the program signs or
  encrypts with one. An RSA key with the fingerprint of the Infineon keys of CVE-2017-15361 (ROCA) is refused the
  same way.
- **Nothing understood that is not.** A header with `crit` (no extension is understood) or with `b64` false (RFC
  7797's unencoded payload) is `errc::unsupported`, and so is a JWE's `zip`: compression is in
  [compress](../compress/README.md), above this module.
- **Errors.** Data that is not a JWS, a JWE or a JWK is `errc::malformed`; a signature that does not verify, a claim
  that does not match, `errc::verification`; a tag that does not match, `errc::authentication`; a token past its
  `exp`, `errc::expired`, and one before its `nbf` or `iat`, `errc::not_yet_valid`.
- **The algorithms.** Signatures: HS256, HS384, HS512 (an oct key), RS256, RS384, RS512 and PS256, PS384, PS512 (RSA,
  the PSS salt as long as the digest), ES256, ES384 and ES512 (P-256, P-384, P-521, the signature r ‖ s), EdDSA
  (Ed25519). Key management: RSA-OAEP and RSA-OAEP-256, A128KW, A192KW, A256KW (AES key wrap), A128GCMKW, A192GCMKW,
  A256GCMKW, `dir`, ECDH-ES and ECDH-ES+A128KW, +A192KW, +A256KW (P-256, P-384, P-521, X25519, the Concat KDF of RFC
  7518 §4.6). Contents: A128CBC-HS256, A192CBC-HS384, A256CBC-HS512, A128GCM, A192GCM,
  A256GCM ([algorithm](jose-algorithm.md), [encryption](jose-encryption.md)). Not here: `none`, RSA1_5 (RFC 8725
  §3.2), PBES2 (a password belongs to a password hash), Ed448 and X448, a JWE's JSON serialization.
- **Secrets.** The private members of a JWK (`d`, `p`, `q`, `dp`, `dq`, `qi`, `k`) are read from its text straight into
  plain memory, by a base64url of the module's own that looks no character up in a table, and written out only into a
  [secret_bytes](secret_bytes/README.md); a key lives in plain memory, zeroed by its own destructor when the last
  handle's state is collected. A decrypted payload is the program's data, a `vector<byte>`. A JWE whose key does not
  unwrap (RSA-OAEP) goes on with a random content key, so that it fails as a wrong tag fails (RFC 7516 §11.5).

## Member types

| Type | Definition |
|---|---|
| [jwk](jose-jwk/README.md) | a key of any kind the module has, public or private, and its JWK members |
| [jwk_set](jose-jwk_set/README.md) | the keys of an issuer: a JWK Set, and what verifies against it |
| [jws](jose-jws/README.md) | a JSON Web Signature: made compact or as JSON, read, verified |
| [jwe](jose-jwe/README.md) | a JSON Web Encryption: made and read compact, decrypted |
| [jwt](jose-jwt/README.md) | a JSON Web Token: claims signed, verified and checked |
| [sign_options](jose-sign_options.md) | the algorithm and the protected header of a signature |
| [encrypt_options](jose-encrypt_options.md) | the algorithm, the content encryption and the header of an encryption |
| [algorithm](jose-algorithm.md) | `alg`: the algorithm of a signature or of a key's management (an enumeration) |
| [encryption](jose-encryption.md) | `enc`: the content encryption of a JWE (an enumeration) |
| [key_type](jose-key_type.md) | `kty`: the kind of a key (an enumeration) |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    // the issuer's key, and what it publishes
    crypto::jose::jwk key(crypto::p256::private_key::generate(), {.kid = "2026-10"});
    crypto::jose::jwk_set published{key.public_key()};

    auto token = crypto::jose::jwt::sign(encoding::json::object({
        {"iss", "https://id.example"}, {"sub", "alice"}, {"aud", "api"},
        {"exp", time::now().unix() + 3600}}), key);

    // the API: the token checked against the issuer's published keys
    auto t = crypto::jose::jwt::verify(token, published,
                                       {.issuer = "https://id.example", .audience = "api"});
    println("{}", t ? t->subject() : t.error().message());

    auto stranger = crypto::jose::jwk(crypto::p256::private_key::generate(), {.kid = "2026-10"});
    auto forged = crypto::jose::jwt::sign(encoding::json::object({{"sub", "mallory"}}), stranger);
    println("{}", crypto::jose::jwt::verify(forged, published).error().message());
}
```

Output:

```text
alice
sgcl::crypto::jose: JWS: the signature does not verify
```

## See also

- [ecdsa](ecdsa.md), [rsa](rsa.md), [ed25519](ed25519.md), [hmac](hmac/README.md): the signatures under JWS
- [aes_gcm](aes_gcm/README.md), [x25519](x25519.md), [p256](p256.md): what JWE encrypts and agrees keys with
- [encoding::json](../encoding/json/README.md): the JSON of headers and claims
- [net::acme](../net/acme/README.md): its requests are JWS made here
- [sgcl::crypto](README.md)
