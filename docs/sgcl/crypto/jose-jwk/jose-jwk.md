[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::jwk

```cpp
explicit jwk(p256::private_key key);                               // (1)
explicit jwk(p256::private_key key, const options& o);             // (1)
explicit jwk(const p256::public_key& key);                         // (2)
explicit jwk(const p256::public_key& key, const options& o);       // (2)
explicit jwk(p384::private_key key);                               // (1)
explicit jwk(p384::private_key key, const options& o);             // (1)
explicit jwk(const p384::public_key& key);                         // (2)
explicit jwk(const p384::public_key& key, const options& o);       // (2)
explicit jwk(p521::private_key key);                               // (1)
explicit jwk(p521::private_key key, const options& o);             // (1)
explicit jwk(const p521::public_key& key);                         // (2)
explicit jwk(const p521::public_key& key, const options& o);       // (2)
explicit jwk(rsa::private_key key);                                // (1)
explicit jwk(rsa::private_key key, const options& o);              // (1)
explicit jwk(const rsa::public_key& key);                          // (2)
explicit jwk(const rsa::public_key& key, const options& o);        // (2)
explicit jwk(ed25519::private_key key);                            // (1)
explicit jwk(ed25519::private_key key, const options& o);          // (1)
explicit jwk(const ed25519::public_key& key);                      // (2)
explicit jwk(const ed25519::public_key& key, const options& o);    // (2)
explicit jwk(x25519::private_key key);                             // (1)
explicit jwk(x25519::private_key key, const options& o);           // (1)
explicit jwk(const x25519::public_key& key);                       // (2)
explicit jwk(const x25519::public_key& key, const options& o);     // (2)
jwk(const jwk& other) noexcept;                                    // (3)
explicit jwk(const string& text);                                  // (4)
```

1. A private key of the module, moved in: the key is moved into plain memory and its source left empty, as a move of
   a key leaves it.
2. A public key, copied.
3. The same key: a copy of the handle. A move is this copy.
4. The key of a JWK's JSON the program spells: [parse](parse.md)'s value. Input is parsed; a text the program itself
   wrote is constructed.

- (1–2) The key's JWK members are its own (`kty`, `crv`, `x`, `y`, `n`, `e`) and those of `o`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |
| `o` | `kid`, `alg` and `use` ([options](../jose-jwk-options.md)) |
| `other` | another jwk |
| `text` | the JSON of one JWK |

## Complexity

Constant, but for an RSA key: linear in its size (its modulus is checked against the fingerprint of CVE-2017-15361);
(4) linear in the text's length besides.

## Exceptions

- (1–2) `std::logic_error` for a key moved from.
- (3) None.
- (4) `bad_expected_access<crypto::error>` with [parse](parse.md)'s error for a text that does not read.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto ed = crypto::ed25519::private_key::generate();
    auto pub = ed.public_key();
    crypto::jose::jwk signer(std::move(ed), {.kid = "k1"});
    crypto::jose::jwk verifier(pub, {.kid = "k1"});
    println("{} {}", signer.is_private(), verifier.is_private());
    println("{}", signer.public_key() == verifier);
}
```

Output:

```text
true false
true
```

## See also

- [generate](generate.md): a new key for an algorithm
- [parse](parse.md): a key of a JWK's JSON
- [sgcl::crypto::jose::jwk](README.md)
