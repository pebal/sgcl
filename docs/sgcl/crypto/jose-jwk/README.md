[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md)

# sgcl::crypto::jose::jwk

```cpp
#include "sgcl/crypto/jose.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::jose {
    class jwk {
    public:
        struct options;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::jose::jwk` is a JSON Web Key (RFC 7517): one key of any kind the module has, public or private —
P-256, P-384, P-521, RSA, Ed25519, X25519 or a symmetric key's octets — with its JWK members (`kid`, `use`, `alg`,
`key_ops`, `x5c` and any other). It is made from a key of the module, generated for an algorithm, or read from a
JWK's JSON or a private key's PEM, and it signs, verifies, encrypts and decrypts in [jws](../jose-jws/README.md),
[jwe](../jose-jwe/README.md) and [jwt](../jose-jwt/README.md). Go's `go-jose` has it as `JSONWebKey`.

## Rules

- **A handle of one word.** A copy is the same key, and a move is a copy: a key moved from stays the key it was. The
  object is immutable: `kid`, `alg` and `use` are given when it is made ([options](../jose-jwk-options.md)) or read.
  Reading from many threads at once is safe.
- **The key lies in plain memory**, never in managed memory: its private half (a scalar, a seed, RSA's numbers, the
  octets) is zeroed by the key's own destructor when the last handle's state is collected, as an
  [account_key](../../net/acme/account_key/README.md) of ACME and a TLS identity keep theirs. The private members are
  read from a JWK's text straight into plain memory and written out only into a
  [secret_bytes](../secret_bytes/README.md).
- **What a key may do** is said by its members: its `alg` pins the one algorithm it works with, its `use` (`sig`,
  `enc`) and `key_ops` limit what it is for; an `alg` the module does not have leaves a key that does nothing
  ([the rules of jose](../jose.md#rules)).

## Member types

| Type | Definition |
|---|---|
| [options](../jose-jwk-options.md) | `kid`, `alg` and `use` of a key made by the program |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](jose-jwk.md) | a key of the module's, private (moved in) or public |
| [symmetric](symmetric.md) | a symmetric key of the program's octets (static) |
| [generate](generate.md) | a new key fit for an algorithm (static) |
| [parse](parse.md) | a JWK's JSON, public or private (static) |
| [from_pem](from_pem.md) | a private key's PEM (static) |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | `kty`: the kind of the key |
| [is_private](is_private.md) | checks whether the key has its private half |
| [kid](kid.md) | `kid` |
| [alg](alg.md) | `alg`: the one algorithm the key is for |
| [use](use.md) | `use`: `sig` or `enc` |
| [crv](crv.md) | `crv`: the curve |
| [parameters](parameters.md) | every public member, as read or made |
| [thumbprint](thumbprint.md) | the JWK thumbprint (RFC 7638) |
| [operator==](operator_cmp.md) | checks whether two are the same key |

#### Conversions

| Function | Description |
|---|---|
| [public_key](public_key.md) | the public half |
| [to_json](to_json.md) | the public JWK's JSON |
| [to_private_json](to_private_json.md) | the JWK's JSON with its private members, in a secret_bytes |
| [to_pem](to_pem.md) | a private key's PKCS #8 PEM, in a secret_bytes |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 7517 A.2's private key
    auto key = crypto::jose::jwk::parse(R"({"kty":"EC","crv":"P-256",
        "x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4",
        "y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM",
        "d":"870MB6gfuTJ4HtUnUvYMyJpr5eUZNP4Bk43bVdj3eAE","use":"enc","kid":"1"})");
    println("{} {} {}", key->crv(), key->kid(), key->is_private());
    println("{}", key->to_json());
    println("{}", key->thumbprint());
}
```

Output:

```text
P-256 1 true
{"kty":"EC","crv":"P-256","x":"MKBCTNIcKUSDii11ySs3526iDZ8AiTo7Tu6KPAqv7D4","y":"4Etl6SRW2YiLUrN5vfvVHuhp7x8PxltmWWlbbM4IFyM","use":"enc","kid":"1"}
cn-I_WNMClehiVp51i_0VpOENW1upEerA8sEam5hn-s
```

## See also

- [jwk_set](../jose-jwk_set/README.md): the keys of an issuer
- [key_type](../jose-key_type.md)
- [sgcl::crypto::jose](../jose.md)
