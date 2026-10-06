[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::mldsa44, mldsa65, mldsa87

```cpp
#include "sgcl/crypto/mldsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::mldsa65 {
    class private_key;
    class public_key;
    struct options;

    inline constexpr size_t public_key_size = 1952;
    inline constexpr size_t signature_size = 3309;
    inline constexpr size_t seed_size = 32;
}

namespace sgcl::crypto::mldsa44 {
    class private_key;
    class public_key;
    struct options;

    inline constexpr size_t public_key_size = 1312;
    inline constexpr size_t signature_size = 2420;
    inline constexpr size_t seed_size = 32;
}

namespace sgcl::crypto::mldsa87 {
    class private_key;
    class public_key;
    struct options;

    inline constexpr size_t public_key_size = 2592;
    inline constexpr size_t signature_size = 4627;
    inline constexpr size_t seed_size = 32;
}
```

ML-DSA (FIPS 204), Go's `crypto/mldsa`: a digital signature on module lattices, believed secure against a quantum
computer as well as a classical one. The owner of a [private_key](mldsa65-private_key/README.md) publishes its
[public_key](mldsa65-public_key/README.md), [signs](mldsa65-private_key/sign.md) messages, and anyone
[verifies](mldsa65-public_key/verify.md) them with the public key; a context string ([options](mldsa65-options.md))
binds a signature to its use. It is what X.509 (RFC 9881) and TLS put beside or in place of ECDSA and Ed25519.

There are three parameter sets, each a namespace of the same three types and three constants: `mldsa44` (security
category 2), `mldsa65` (category 3, the one to use) and `mldsa87` (category 5). The types are documented once, under
`mldsa65`; the sets differ in their sizes alone. A type of one set is never taken for another's, and a signature of
one set does not verify under a key of another.

Written from FIPS 204, tested against Wycheproof's vectors of the three sets (key generation from seeds, deterministic
signatures of messages with contexts and of message representatives, signatures of expanded keys, verification with
malformed hints, norms and lengths), and against Go's `crypto/mldsa` both ways: the same public keys and
deterministic signatures byte for byte, each side verifying the other's hedged signatures. **The implementation has
not been through an independent cryptographic audit.**

## Rules

- **The sizes** of the sets are the constants of their namespaces, in bytes:

  | Constant | mldsa44 | mldsa65 | mldsa87 | Description |
  |---|---|---|---|---|
  | `public_key_size` | 1312 | 1952 | 2592 | the public key, as `bytes()` gives it and `from_bytes` takes it |
  | `signature_size` | 2420 | 3309 | 4627 | a signature, the only length `verify` takes |
  | `seed_size` | 32 | 32 | 32 | the seed ξ a private key is kept as |

- **Signatures are hedged by default**: each draws 32 bytes of [crypto::random](random/README.md) mixed with the key
  and the message (FIPS 204 §3.4), so that two signatures of a message differ; `options::deterministic` makes the
  randomness zeros and one message one signature, Go's `SignDeterministic`. Both verify alike.
- **The pure scheme with a context**: the message is signed as it is, under a context string of at most 255 bytes,
  empty by default (FIPS 204 §5.2). Not here: HashML-DSA (the message hashed first, §5.4) and an external message
  representative μ (RFC 9881).
- **A private key is a secret, kept as its seed**: the seed ξ of 32 bytes and the expanded key made of it once live in
  plain memory of the key's own, never in managed memory, zeroed when the key goes. The key is move-only, `clone()`
  makes a second one, and `seed()` is a [secret\<32\>](secret/README.md). There is no DER and no expanded form: PKCS #8
  and SubjectPublicKeyInfo of ML-DSA (RFC 9881) come with the public ASN.1 of the encoding module.
- **A public key is a handle of one word** to a state made once (the matrix Â and t1 transformed), so that copies are
  free and verifications only hash and multiply. Every string of the right length is a key.
- **What is secret**: the seed, ρ', K, s1, s2, t0, the masks y of each round and everything made of them. **Constant
  time** wherever they are: the ring's arithmetic is Montgomery's reduction and masks, the rounding (Power2Round,
  Decompose, the hints) multiplications by constants and masks, the norm checks a flag gathered over every
  coefficient; what held a secret is zeroed before a function returns. What branches is public by FIPS 204's design:
  whether a signing round is rejected (a rejected round reveals nothing but that), the places of c's ±1 (c is a part
  of the signature), the rejection sampling of Â (from the public ρ); and, as in every implementation of the standard,
  the count of bytes the rejection sampling of s1 and s2 passes over during key generation.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the signer's key; it publishes the public key's bytes
    auto key = crypto::mldsa65::private_key::generate();
    vector<byte> published = key.public_key().bytes();
    println("public key: {} bytes", published.size());

    // a signature of a message, under the application's context
    auto signature = key.sign("release 1.0.0", {.context = "builds"});
    println("signature: {} bytes", signature.size());

    // anyone with the public key verifies it
    auto verifier = crypto::mldsa65::public_key::from_bytes(published).value();
    println("{}", verifier.verify("release 1.0.0", signature, {.context = "builds"}));
    println("{}", verifier.verify("release 1.0.1", signature, {.context = "builds"}));
    println("{}", verifier.verify("release 1.0.0", signature));
}
```

Output:

```text
public key: 1952 bytes
signature: 3309 bytes
true
false
false
```

## See also

- [mldsa65::private_key](mldsa65-private_key/README.md): the signer's secret key
- [mldsa65::public_key](mldsa65-public_key/README.md): the published key
- [mldsa65::options](mldsa65-options.md): the context and the determinism
- [ed25519](ed25519.md), [ecdsa](ecdsa.md): the classical signatures
- [mlkem768](mlkem.md): the post-quantum key exchange
- [random](random/README.md), [secret](secret/README.md), [error](error/README.md)
