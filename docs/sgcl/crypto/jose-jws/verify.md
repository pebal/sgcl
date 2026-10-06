[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jws](README.md)

# sgcl::crypto::jose::jws::verify

```cpp
static expected<vector<byte>, error> verify(const string& text, const jwk& key) noexcept;         // (1)
static expected<vector<byte>, error> verify(const string& text, const jwk_set& keys) noexcept;    // (2)
expected<vector<byte>, error> verify(const jwk& key) const noexcept;                              // (3)
expected<vector<byte>, error> verify(const jwk_set& keys) const noexcept;                         // (4)
```

The payload, when a signature of the JWS verifies under the key, with an algorithm the key allows: the key's `alg`
when it has one, else the algorithms of its kind ([the rules of jose](../jose.md#rules)). Of several signatures, one
that verifies is enough.

- (1–2) Read the text first, as [parse](parse.md) reads it: the one line.
- (3–4) This JWS, read before.
- (2), (4) Under a key of the set: the keys of the signature's `kid` when it names one, every key when it does not. A
  set that mixes symmetric keys with public ones is refused, and so is a kid that two keys have.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the JWS, compact or JSON |
| `key` | the key: a public key, a private one (its public half verifies), or an oct key |
| `keys` | the keys |

## Return value

The payload, or an error: what [parse](parse.md) refuses; `errc::invalid_key` for a key that may not verify (its
`use`, `key_ops` or `alg` forbid it, an HMAC key shorter than the digest, RSA under 2048 bits, the set refused);
`errc::verification` for a signature that does not verify, or an algorithm the key does not allow (`none` among
them).

## Complexity

Linear in the length of the JWS, and the verification.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::rs256);
    auto token = crypto::jose::jws::sign("transfer 10", key);
    println("{}", string(crypto::jose::jws::verify(token, key.public_key()).value()));

    // a token claiming HS256 under the public key's JSON as an HMAC key: the key is RS256's
    auto confused = crypto::jose::jwk::symmetric(key.public_key().to_json());
    auto forged = crypto::jose::jws::sign("transfer 10000", confused);
    println("{}", crypto::jose::jws::verify(forged, key.public_key()).error().message());
}
```

Output:

```text
transfer 10
sgcl::crypto::jose: JWS: the key is for another algorithm
```

## See also

- [jwt::verify](../jose-jwt/verify.md): a token's signature and claims
- [sgcl::crypto::jose::jws](README.md)
