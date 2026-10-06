[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::parse

```cpp
static expected<jwk, error> parse(const string& text) noexcept;
static expected<jwk, error> parse(const secret_bytes& text) noexcept;
```

The key of a JWK's JSON (RFC 7517 §4, RFC 7518 §6, RFC 8037 §2), public or private. The private members (`d`, `p`,
`q`, `dp`, `dq`, `qi`, `k`) are decoded where they lie in the text straight into plain memory, never through managed
memory, so the text may be the [secret_bytes](../secret_bytes/README.md) of [read_secret](../read_secret.md); the
public members go to an [encoding::json](../../encoding/json/README.md) and are kept, the unknown ones too
([parameters](parameters.md)). A key is checked as its type checks one: a point on its curve, `d` the scalar of
`x` and `y`, a seed of `x`, RSA's numbers agreeing.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the JSON of one JWK: a string, or a private key's in a secret_bytes |

## Return value

The key, or an error:

- `errc::malformed`: text that is not one JSON object, a member given twice, no `kty`, a member of the wrong type, a
  private member that is not a base64url string (with no escape in it), an EC or OKP key without `crv`;
- `errc::unsupported`: a `kty` or a curve the module does not have (secp256k1, Ed448, X448), a multi-prime RSA key, an
  RSA private key without its primes;
- `errc::invalid_key`: a key that cannot be one.

## Complexity

Linear in the length of the text, and the check of the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8037 A.2: the public key of A.1
    auto pub = crypto::jose::jwk::parse(
        R"({"kty":"OKP","crv":"Ed25519","x":"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo"})");
    println("{} {}", pub->crv(), pub->is_private());
    auto bad = crypto::jose::jwk::parse(R"({"kty":"EC","crv":"secp256k1","x":"AA","y":"AA"})");
    println("{}", bad.error().message());
}
```

Output:

```text
Ed25519 false
sgcl::crypto::jose: JWK: an EC key of a curve the module does not have
```

## See also

- [to_private_json](to_private_json.md), [to_json](to_json.md): the other way
- [jwk_set::parse](../jose-jwk_set/parse.md): the keys of a set
- [sgcl::crypto::jose::jwk](README.md)
