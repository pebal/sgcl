# sgcl::crypto

Cryptography: what Go has in `crypto/sha1`, `crypto/sha256`, `crypto/sha512`, `crypto/sha3`, `crypto/hmac`, `crypto/hkdf`, `crypto/pbkdf2`, `crypto/rand`, `crypto/subtle`, `crypto/aes`, `crypto/cipher`, `golang.org/x/crypto/chacha20poly1305`, `crypto/ecdh`, `crypto/ecdsa`, `crypto/ed25519`, `crypto/mlkem`, `crypto/rsa` and `crypto/x509`. `#include "sgcl/crypto/crypto.h"` brings the module in; it depends on [`core`](../core/README.md), on [`hash`](../hash/README.md), whose shape of a hasher its digests take (`update`, `value`, `of`, `copy_from`), and, for X.509, on [`io`](../io/README.md) (the system's roots from their files), [`time`](../time/README.md) (a certificate's validity) and [`encoding`](../encoding/README.md) (PEM). `net` will take its TLS 1.3 from here. The index of the whole interface is [`docs/sgcl/`](../README.md).

**The implementation has not been through an independent cryptographic audit.** Every algorithm is written from its specification (FIPS 180-4, 197, 198-1, 202, 186-5, SP 800-38A and 38D, RFC 5869, 6979, 7748, 8017, 8018, 8032, 8439, 5280, 6125, SEC 1, X.690) and held in the tests to the specifications' vectors and, on random data, to OpenSSL 3 and Go; OpenSSL, Go and Python are the tests' oracles and nothing more.

## What is here

| type | Go | what it is for |
|---|---|---|
| [`sha1`](sha1.md) | `crypto/sha1` | Git's object names, old protocols; broken for signatures |
| [`sha224`, `sha256`](sha256.md) | `crypto/sha256` | the digest of TLS, certificates, JWTs, Bitcoin |
| [`sha384`, `sha512`, `sha512_256`](sha512.md) | `crypto/sha512` | the larger digests; Ed25519's |
| [`sha3_224` … `sha3_512`, `shake128`, `shake256`](sha3.md) | `crypto/sha3` | Keccak: the other family, and output of any length |
| [`hash_id`](hash_id.md) | `crypto.Hash` | a digest named by data: a certificate's algorithm |
| [`hmac`](hmac.md) | `crypto/hmac` | a tag under a key: API signatures, cookies, JWTs (HS256) |
| [`hkdf`](hkdf.md) | `crypto/hkdf` | keys from a secret: TLS 1.3's key schedule, after ECDH |
| [`pbkdf2`](pbkdf2.md) | `crypto/pbkdf2` | keys from a password (not for storing passwords, below) |
| [`random`](random.md) | `crypto/rand` | the system's random bytes: keys, nonces, salts, tokens |
| [`constant_time`](constant_time.md) | `crypto/subtle` | comparing a tag without telling how much of it was right |
| [`secure_zero`](secure_zero.md) | — | clearing a buffer that held a secret |
| [`aes_gcm`](aes_gcm.md) | `cipher.NewGCM` | the AEAD of TLS, on the processor's AES instructions |
| [`chacha20_poly1305`, `xchacha20_poly1305`](chacha20_poly1305.md) | `chacha20poly1305` | the other AEAD; XChaCha's 24-byte nonce may be random |
| [`aead`](aead.md) | `cipher.AEAD` | the interface the AEADs share: `seal`, `open`, `seal_to`, `open_to` |
| [`nonce_counter`](nonce_counter.md) | — | nonces that never repeat under one key |
| [`aes`](aes.md), [`aes_ctr`](aes_ctr.md), [`chacha20`](chacha20.md) | `crypto/aes`, `cipher.NewCTR`, `chacha20` | the bare ciphers, unauthenticated: for protocols that authenticate otherwise |
| [`x25519`](x25519.md) | `crypto/ecdh.X25519()` | key agreement: TLS 1.3, SSH, WireGuard, Signal |
| [`ed25519`](ed25519.md) | `crypto/ed25519` | signatures: SSH keys, certificates, packages, JWTs (EdDSA) |
| [`mlkem512`, `mlkem768`, `mlkem1024`](mlkem.md) | `crypto/mlkem` | ML-KEM (FIPS 203), post-quantum key encapsulation: TLS 1.3's X25519MLKEM768 |
| [`p256`](p256.md), [`p384`](p384.md) | `crypto/ecdh`, `crypto/ecdsa` | the NIST curves: ECDH and ECDSA of TLS, X.509, WebAuthn, JWTs (ES256) |
| [`ecdsa`](ecdsa.md) | `crypto/ecdsa` | how the curves sign and verify: hedged RFC 6979 nonces, DER and raw signatures |
| [`rsa`](rsa.md) | `crypto/rsa` | signatures of certificates and JWTs (PKCS #1 v1.5, PSS: RS256, PS256), OAEP key transport; PKCS #1, PKCS #8, SPKI |
| [`x509`](x509.md) | `crypto/x509` | certificates: `certificate` from DER or PEM, `certificate_pool` (the system's roots), `verify` of a chain with the host name or IP address, the reason when it fails; no CRL or OCSP |
| [`secret`, `secret_bytes`, `read_secret`](secret.md) | — | secrets that zero themselves and never lie in managed memory: of a fixed size (a shared secret, a private scalar) and of a size known when the program runs (a derived key, a private key's export, a key file) |
| [`error`](error.md) | — | the one error of the module: `code()`, `offset()`, `message()` |

TLS 1.3 comes next, in `net`.

## The rules of the module

- **Errors.** What comes with data and may be wrong is an [`expected<T, crypto::error>`](error.md): a tag that does not match (`errc::authentication`, and nothing more said), a key read from bytes that cannot be one (`invalid_key`), a signature that is not one in form, DER that cannot be read (`malformed`, with the offset), another algorithm's key (`unsupported`). A broken contract of the program — a key of the wrong length written into it, a nonce of the wrong size, an output buffer too small — is an exception, `std::invalid_argument` or `std::length_error`, as everywhere in the library; each type's `from_key` or `from_bytes` is the form for a key that comes with data.
- **Checked before `[[nodiscard]]` is ignored.** `open`, `open_to`, every `verify` and `constant_time::equal` are `[[nodiscard]]`: a verification whose result is dropped is a hole. `open` checks the tag before a byte is decrypted and zeroes what it would have written when the tag does not match.
- **A secret is never in managed memory.** Managed memory is not zeroed when an object dies: a block the collector frees keeps its bytes until it is given out again. So nothing the module holds or gives that is a secret goes there. Three forms carry secrets:
  - **[`secret<N>`](secret.md)**, a length known when the program is compiled: a shared secret, a private scalar, a key.
  - **[`secret_bytes`](secret.md#secret_bytes)**, a length known only when it runs, in 64 bytes of its own and past them in a block that is zeroed before it is freed (and when it grows): what `hkdf` and `pbkdf2` derive, SHAKE's output, `random::secret(n)`, every private key's `to_pkcs8_der`, `to_sec1_der`, `to_pkcs1_der` and `to_pem`, and a key file read by `read_secret`. It is move-only; `clone()` is the copy by name.
  - **The `_to` forms**, which write into the program's own buffer: `derive_to`, `open_to`, `read_to`, `decrypt_oaep_to`.
  - **Not a secret:** a plaintext. What an AEAD's `open` and `open_random` give and what `rsa::decrypt_oaep` decrypts is the user's data, a `vector<byte>`; a key unwrapped goes through `open_to` or `decrypt_oaep_to` into a buffer the program clears (a `secret_bytes`).

  A key object holds its bytes in itself (no allocation): the round keys, the seed, the scalar. It is move-only, and its destructor and a move out of it overwrite those bytes with stores the compiler cannot remove. So a key and a `secret_bytes` belong **on the stack or in a `unique_ptr`**; in a managed object they would stay in memory until the collector's cycle found the object dead.

  Private keys read from PEM go the same way: `from_pem` takes bytes and decodes their base64 straight into a `secret_bytes`. So do [`net::tls::identity`](../net/tls.md) (its key's PEM) and [`compress::sevenzip::options::password`](../compress/sevenzip.md#extract) (a password). A key file:

  ```cpp
  crypto::ed25519::private_key key = crypto::ed25519::private_key::from_pem(crypto::read_secret("signing.key"));
  ```

  The tests hold the rule. After a TLS exchange and a 7z archive read with a password, the managed pages are searched: none holds the key, a traffic secret or the password. The exports and imports of every key take no managed page over thousands of calls, and every block a `secret_bytes` frees is zero.
- **Nonces.** An AEAD's nonce must never repeat under one key: a repeated nonce under AES-GCM gives away the authentication key, and under both AEADs the XOR of the two plaintexts. Where the two sides count their messages, a [`nonce_counter`](nonce_counter.md) makes the nonces and refuses to wrap. Random nonces are safe only with 24 bytes: [`xchacha20_poly1305::seal_random`](chacha20_poly1305.md) draws one and writes it in front. Twelve random bytes (AES-GCM, ChaCha20-Poly1305) are safe for about 2^32 messages under one key, as SP 800-38D says, and not beyond.
- **Passwords.** [`pbkdf2`](pbkdf2.md) derives a key from a password (a file's encryption key, a protocol that names it). It is not the way to **store** passwords: it costs an attacker's graphics card as little as it costs the server, and a password hash for storage wants one that costs memory, Argon2id or scrypt, which come after version 1.
- **Constant time.** Nothing that depends on a secret chooses a branch or an address: the AES S-box is the processor's or computed bitsliced, never a table; the curves' ladders and table scans go through masks; the inverses are Fermat's (RSA's one inverse, of its blinding factor, is taken of a product with a second random number); RSA's private operation runs blinded and CRT over words of a width fixed by the key; tags are compared by [`constant_time::equal`](constant_time.md). What is public — a length, a public key, whether a verification passed — may take its own time. The tests measure the claim for the curves and RSA with dudect, on this machine only; the machine code of the rest has been read for branches and loads that depend on data.

## Two roads: the processor's instructions and plain C++

On arm64 the module runs on the processor's own instructions: SHA-1 and SHA-256 (`FEAT_SHA1`, `FEAT_SHA256`), SHA-512 and SHA-3 (`FEAT_SHA512`, `FEAT_SHA3`), AES (`AESE`, `AESMC`) and GHASH (`PMULL`), and NEON for ChaCha20. Everywhere else, and in a build with `SGCL_CRYPTO_PORTABLE` defined, every algorithm has a body in plain C++ that gives the same bytes, in constant time too: AES bitsliced, GHASH on integer products — correct and constant-time, and two orders of magnitude slower than the instructions. The tests run every vector on both roads (`tests_crypto` and `tests_crypto_portable`).

No build flag is needed for the fast roads. Their bodies are compiled on every target with a target attribute, and the processor is asked once, at the first call, which instructions it has (`sysctlbyname` on macOS, `getauxval` on Linux, `cpuid` on x86-64): so a program built for plain `armv8-a` or plain x86-64 still uses the instructions where the processor has them, and takes the portable road where it has not. Where the target itself promises the instructions, as Apple's does, the question folds away at compile time. A cipher's road is chosen with its key, from the processor alone.

| road | arm64 | x86-64 |
|---|---|---|
| AES (GCM, CTR, CBC decryption), GHASH | AESE/AESMC and PMULL, eight blocks at a time | AES-NI and PCLMULQDQ, eight blocks at a time |
| SHA-1, SHA-256 | the SHA1/SHA256 instructions | SHA-NI |
| SHA-512, SHA-3 | the SHA512 and SHA3 instructions | portable |
| ChaCha20 | NEON, four and eight blocks | SSE2, four blocks; AVX2, eight |
| Poly1305 | 64-bit multiplications | 64-bit multiplications |

On x86-64 the SHA-NI and AVX2 roads are compiled but not yet run on a processor that has them (the machines this is built on emulate x86-64 without them); until they are, they are unverified, and the x86 machine closes them. AES-NI, PCLMULQDQ and the SSE2 ChaCha20 pass every vector of the suite.

## Pages

[`sha1`](sha1.md) · [`sha256`](sha256.md) · [`sha512`](sha512.md) · [`sha3`](sha3.md) · [`hash_id`](hash_id.md) · [`hmac`](hmac.md) · [`hkdf`](hkdf.md) · [`pbkdf2`](pbkdf2.md) · [`random`](random.md) · [`constant_time`](constant_time.md) · [`secure_zero`](secure_zero.md) · [`aead`](aead.md) · [`aes_gcm`](aes_gcm.md) · [`chacha20_poly1305`](chacha20_poly1305.md) · [`nonce_counter`](nonce_counter.md) · [`aes`](aes.md) · [`aes_ctr`](aes_ctr.md) · [`chacha20`](chacha20.md) · [`x25519`](x25519.md) · [`ed25519`](ed25519.md) · [`mlkem`](mlkem.md) · [`p256`](p256.md) · [`p384`](p384.md) · [`ecdsa`](ecdsa.md) · [`rsa`](rsa.md) · [`x509`](x509.md) · [`secret`](secret.md) · [`error`](error.md) · [benchmarks](benchmarks.md)
