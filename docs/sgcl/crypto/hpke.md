[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::hpke

```cpp
#include "sgcl/crypto/hpke.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto::hpke {
    enum class kem : uint16_t;
    enum class kdf : uint16_t;
    enum class aead : uint16_t;
    struct suite;
    class public_key;
    class private_key;
    struct options;
    class sender;
    class recipient;

    expected<vector<byte>, error> seal(const public_key& to, const slice<const byte>& plaintext,
                                       const suite& s = {}, const slice<const byte>& info = {},
                                       const slice<const byte>& aad = {});
    expected<vector<byte>, error> open(const private_key& key, const slice<const byte>& sealed,
                                       const suite& s = {}, const slice<const byte>& info = {},
                                       const slice<const byte>& aad = {});
}
```

`sgcl::crypto::hpke` is HPKE, Hybrid Public Key Encryption (RFC 9180): messages sealed to a recipient's public key.
A KEM gives the sender and the recipient one shared secret — the sender from the recipient's
[public key](hpke-public_key/README.md) and an ephemeral key of its own, the recipient from its
[private key](hpke-private_key/README.md) and the `enc` the sender sends — and a key schedule turns it into an AEAD's
key, its nonces and a secret to export from. A [sender](hpke-sender/README.md) seals messages in order, a
[recipient](hpke-recipient/README.md) opens them in the same order; [seal](hpke-seal.md) and [open](hpke-open.md) do one
message in one line, `enc` in front of the ciphertext. What TLS's Encrypted Client Hello
([net::tls](../net/tls/README.md)), MLS, Oblivious HTTP and DNS seal with; Go's `crypto/hpke`.

Written from RFC 9180; tested against its base-mode vectors as Go keeps them (a thousand seals and a thousand exports
of each suite, accumulated), both ways against Go's `crypto/hpke` in every suite here, and the PSK, auth and auth-PSK
modes, which Go does not have, against RFC 9180 written by hand in Python. **The implementation has not been through
an independent cryptographic audit.**

## Rules

- **A ciphersuite** is a KEM, a KDF and an AEAD, named by their ids: the KEM is the key's — DHKEM(X25519,
  HKDF-SHA256), DHKEM(P-256, HKDF-SHA256), DHKEM(P-384, HKDF-SHA384) or DHKEM(P-521, HKDF-SHA512) — and the [suite](hpke-suite.md) names the
  other two: HKDF-SHA256, -SHA384, -SHA512; AES-128-GCM, AES-256-GCM, ChaCha20-Poly1305 or export-only. Both sides
  must use the same suite and the same `info`.
- **The mode follows from what the setup is given**: a pre-shared key in its [options](hpke-options.md) makes it PSK,
  the sender's key (a parameter of its own) auth, both auth-PSK, neither base.
- **Order.** The sender's n-th message is sealed under the n-th nonce and the recipient opens it as its n-th; a message
  that does not open leaves the recipient where it was.
- **Secrets.** A context's key, base nonce and exporter secret, and a private key's scalar, lie in the object and are
  zeroed when it goes; the contexts and the private keys are move-only; an exported secret is a
  [secret_bytes](secret_bytes/README.md).
- **Errors.** A key or an `enc` that is not one of the KEM, an X25519 point of small order, is `errc::invalid_key`; a
  message that does not open, `errc::authentication`; a PSK without its id, `errc::malformed` (the sender's side:
  `std::invalid_argument`).
- **Not here**: the post-quantum KEMs of draft-ietf-hpke-pq (X-Wing, ML-KEM and its hybrids) and the SHAKE KDFs, which
  are drafts yet; DHKEM(X448).

## Member types

| Type | Definition |
|---|---|
| [kem](hpke-kem.md) | the KEM of a suite (an enumeration) |
| [kdf](hpke-kdf.md) | the KDF of a suite (an enumeration) |
| [aead](hpke-aead.md) | the AEAD of a suite (an enumeration) |
| [suite](hpke-suite.md) | the KDF and the AEAD of a ciphersuite |
| [public_key](hpke-public_key/README.md) | a KEM public key: the recipient's, or a sender's of the auth modes |
| [private_key](hpke-private_key/README.md) | a KEM private key, move-only, zeroed when it goes |
| [options](hpke-options.md) | the pre-shared key of the PSK modes |
| [sender](hpke-sender/README.md) | a sending context: enc, sealed messages, exports |
| [recipient](hpke-recipient/README.md) | a receiving context: opened messages, exports |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto sealed = crypto::hpke::seal(key.public_key(), "meet at noon", {}, "app/v1");
    auto opened = crypto::hpke::open(key, sealed.value(), {}, "app/v1");
    println("{} bytes sealed: {}", sealed->size(), string(opened.value()));
}
```

Output:

```text
60 bytes sealed: meet at noon
```

## See also

- [x25519](x25519.md), [p256](p256.md): the curves under the KEMs
- [aes_gcm](aes_gcm/README.md), [chacha20_poly1305](chacha20_poly1305/README.md): the AEADs
- [hkdf](hkdf/README.md): the KDF
- [jose::jwe](jose-jwe/README.md): the other way to encrypt to a public key, in JSON
- [sgcl::crypto](README.md)
