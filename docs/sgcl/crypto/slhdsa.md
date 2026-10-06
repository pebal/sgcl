[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::slhdsa_sha2_128s ... slhdsa_shake_256f

```cpp
#include "sgcl/crypto/slhdsa.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::slhdsa_sha2_128s {
    class private_key;
    class public_key;
    struct options;

    inline constexpr size_t public_key_size = 32;
    inline constexpr size_t private_key_size = 64;
    inline constexpr size_t signature_size = 7856;
}

// the same in each of the other eleven sets:
// slhdsa_sha2_128f, slhdsa_sha2_192s, slhdsa_sha2_192f, slhdsa_sha2_256s, slhdsa_sha2_256f,
// slhdsa_shake_128s, slhdsa_shake_128f, slhdsa_shake_192s, slhdsa_shake_192f, slhdsa_shake_256s, slhdsa_shake_256f
```

SLH-DSA (FIPS 205), the stateless hash-based digital signature (SPHINCS+): a signature whose security rests on its hash
function alone, believed secure against a quantum computer; signatures are large and slow to make, keys tiny. It
suits what is signed rarely and checked often or for long — firmware, releases, roots of trust — beside
[ML-DSA](mldsa.md), whose signatures are smaller and faster. The owner of a
[private_key](slhdsa_sha2_128s-private_key/README.md) publishes its [public_key](slhdsa_sha2_128s-public_key/README.md),
[signs](slhdsa_sha2_128s-private_key/sign.md) messages, and anyone [verifies](slhdsa_sha2_128s-public_key/verify.md)
them; a context string ([options](slhdsa_sha2_128s-options.md)) binds a signature to its use.

There are twelve parameter sets, each a namespace of the same three types and three constants: the hash (SHA-2 or
SHAKE), the security category (128: 1, 192: 3, 256: 5), and small signatures with slow signing (`s`) or fast signing
with larger signatures (`f`). The types are documented once, under `slhdsa_sha2_128s`. Go has no SLH-DSA; tested
against OpenSSL 3.6 in all twelve sets: the public and private keys of the same seeds, the deterministic signatures of
messages with contexts byte for byte, and each side verifying the other's hedged signatures. **The implementation has
not been through an independent cryptographic audit.**

## Rules

- **The sizes** of the sets, in bytes (the private key is 2 × the public key):

  | Set | `public_key_size` | `signature_size` | | Set | `public_key_size` | `signature_size` |
  |---|---|---|---|---|---|---|
  | `slhdsa_sha2_128s` | 32 | 7856 | | `slhdsa_shake_128s` | 32 | 7856 |
  | `slhdsa_sha2_128f` | 32 | 17088 | | `slhdsa_shake_128f` | 32 | 17088 |
  | `slhdsa_sha2_192s` | 48 | 16224 | | `slhdsa_shake_192s` | 48 | 16224 |
  | `slhdsa_sha2_192f` | 48 | 35664 | | `slhdsa_shake_192f` | 48 | 35664 |
  | `slhdsa_sha2_256s` | 64 | 29792 | | `slhdsa_shake_256s` | 64 | 29792 |
  | `slhdsa_sha2_256f` | 64 | 49856 | | `slhdsa_shake_256f` | 64 | 49856 |

- **Signatures are hedged by default**: each draws n bytes of [crypto::random](random/README.md) into its randomizer,
  so that two signatures of a message differ; `options::deterministic` uses PK.seed instead (FIPS 205 §10.2.1) and
  gives one message one signature. Both verify alike.
- **The pure scheme with a context**: the message is signed as it is, under a context string of at most 255 bytes,
  empty by default (§10.2). Not here: HashSLH-DSA (the message hashed first, §10.2.2).
- **Keys are values in their objects**: the public key's 2n bytes copy freely; the private key's 4n bytes (SK.seed,
  SK.prf, PK.seed, PK.root, OpenSSL's raw form, a [secret\<N\>](secret/README.md) of `bytes()`) are move-only, zeroed
  when the key goes, never in managed memory, and read back by `from_bytes`, which recomputes PK.root of the seeds and refuses one that does not match. There is no
  DER: PKCS #8 and SubjectPublicKeyInfo of SLH-DSA come with the public ASN.1 of the encoding module.
- **What is secret, and constant time**: SK.seed, SK.prf and every WOTS+ and FORS value derived of them. They only go
  through the hash functions, whose time does not depend on their bytes; every index, chain length and branch comes
  from the digest of the randomizer and the message, which the signature makes public. What held a secret is zeroed
  before a function returns; `operator==` compares in constant time.
- **The hashes**: the SHA-2 sets hash PK.seed padded to a block once per key, so that each of their millions of hashes
  costs the compression of what follows it, on the processor's SHA-256 and SHA-512 instructions where it has them;
  the SHAKE sets put a single-block input into the Keccak state lane by lane, on the SHA-3 instructions where the
  processor has them. Signing takes 90 ms for SHA2-128s and 5 ms for SHA2-128f on the machine of the tests; the
  SHAKE sets take about four times as long.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the signer's key; it publishes the public key's 32 bytes
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    vector<byte> published = key.public_key().bytes();
    println("public key: {} bytes", published.size());

    // a signature of a release, under the application's context
    auto signature = key.sign("release 1.0.0", {.context = "releases"});
    println("signature: {} bytes", signature.size());

    // anyone with the public key verifies it
    auto verifier = crypto::slhdsa_sha2_128s::public_key::from_bytes(published).value();
    println("{}", verifier.verify("release 1.0.0", signature, {.context = "releases"}));
    println("{}", verifier.verify("release 1.0.1", signature, {.context = "releases"}));
}
```

Output:

```text
public key: 32 bytes
signature: 7856 bytes
true
false
```

## See also

- [slhdsa_sha2_128s::private_key](slhdsa_sha2_128s-private_key/README.md): the signer's secret key
- [slhdsa_sha2_128s::public_key](slhdsa_sha2_128s-public_key/README.md): the published key
- [slhdsa_sha2_128s::options](slhdsa_sha2_128s-options.md): the context and the determinism
- [mldsa65](mldsa.md): the lattice signature, smaller and faster
- [ed25519](ed25519.md), [ecdsa](ecdsa.md): the classical signatures
- [random](random/README.md), [secret_bytes](secret_bytes/README.md), [error](error/README.md)
