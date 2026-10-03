[sgcl](../README.md) › crypto

# sgcl::crypto

```cpp
#include "sgcl/crypto.h"   // namespace sgcl::crypto
```

Cryptography: what Go has in `crypto/sha1`, `crypto/sha256`, `crypto/sha512`, `crypto/sha3`, `crypto/hmac`,
`crypto/hkdf`, `crypto/pbkdf2`, `crypto/rand`, `crypto/subtle`, `crypto/aes`, `crypto/cipher`,
`golang.org/x/crypto/chacha20poly1305`, `crypto/ecdh`, `crypto/ecdsa`, `crypto/ed25519`, `crypto/mlkem`,
`crypto/rsa` and `crypto/x509`: the digests and the hash ids, HMAC, HKDF and PBKDF2, random bytes for keys and
nonces, comparison in constant time, the AEADs and the bare ciphers under them, key agreement (X25519, ECDH on
P-256 and P-384, ML-KEM), signatures (Ed25519, ECDSA, RSA) and X.509 certificates with the verification of their
chains. The module depends on [core](../core/README.md), on [hash](../hash/README.md), whose shape of a hasher its
digests take (`update`, `value`, `of`, `copy_from`), and, for X.509, on [io](../io/README.md) (the system's roots
from their files), [time](../time/README.md) (a certificate's validity), [encoding](../encoding/README.md) (PEM)
and [async](../async/README.md) (the pool's forms for a task). [net::tls](../net/tls/README.md) takes its TLS 1.3 from
here. The index of the whole interface is [the modules](../README.md).

The idea the module rests on is that what comes with data is a value and what is secret never lies where the
collector leaves it. A tag that does not match, a key that cannot be one, DER that cannot be read are an
`expected<T, crypto::error>`; a broken contract of the program is an exception. A key holds its bytes in itself,
moves rather than copies, and overwrites them when it goes; a secret of a length known only when the program runs
is a `secret_bytes`, in plain memory zeroed before it is freed, never in managed memory, which keeps the bytes of a
dead object until its block is given out again. Nothing that depends on a secret chooses a branch or an address.

**The implementation has not been through an independent cryptographic audit.** Every algorithm is written from
its specification (FIPS 180-4, 197, 198-1, 202, 203, 186-5, SP 800-38A and 38D, RFC 5869, 6979, 7748, 8017,
8018, 8032, 8439, 5280, 6125, SEC 1, X.690) and held in the tests to the specifications' vectors and, on random
data, to OpenSSL 3 and Go; OpenSSL, Go and Python are the tests' oracles and nothing more.

## The rules

**Errors.** What comes with data and may be wrong is an [expected](../core/expected/README.md) whose error is a
[crypto::error](error/README.md): a tag that does not match (`errc::authentication`, and nothing more said), a key read
from bytes that cannot be one (`invalid_key`), a signature that is not one in form (`invalid_signature`), DER that
cannot be read (`malformed`, with the offset), another algorithm's key (`unsupported`), a chain that does not
verify (`verification`, with its [x509::reason](x509-reason.md)). A broken contract of the program — a key of the
wrong length written into it, a nonce of the wrong size, an output buffer too small, a key used after it was moved
from — is an exception, `std::invalid_argument`, `std::length_error` or `std::logic_error`, as everywhere in the
library; each type's `from_key` or `from_bytes` is the form for a key that comes with data.

**Checked before `[[nodiscard]]` is ignored.** `open`, `open_to`, every `verify` and `constant_time::equal` are
`[[nodiscard]]`: a verification whose result is dropped is a hole. `open` checks the tag before a byte is decrypted
and zeroes what it would have written when the tag does not match.

**A secret is never in managed memory.** Managed memory is not zeroed when an object dies: a block the collector
frees keeps its bytes until it is given out again. So nothing the module holds or gives that is a secret goes
there. Three forms carry secrets:

- [secret\<N\>](secret/README.md), a length known when the program is compiled: a shared secret, a private scalar, a key.
- [secret_bytes](secret_bytes/README.md), a length known only when it runs, in 64 bytes of its own and past them in a
  block that is zeroed before it is freed (and when it grows): what `hkdf` and `pbkdf2` derive, SHAKE's output,
  `random::secret(n)`, every private key's `to_pkcs8_der`, `to_sec1_der`, `to_pkcs1_der` and `to_pem`, and a key
  file read by [read_secret](read_secret.md). It is move-only; `clone()` is the copy by name.
- The `_to` forms, which write into the program's own buffer: `derive_to`, `open_to`, `read_to`,
  `decrypt_oaep_to`.

A plaintext is not a secret: what an AEAD's `open` and `open_random` give and what `rsa::private_key::decrypt_oaep`
decrypts is the user's data, a `vector<byte>`; a key unwrapped goes through `open_to` or `decrypt_oaep_to` into a
buffer the program clears (a `secret_bytes`).

A key object holds its bytes in itself (no allocation): the round keys, the seed, the scalar. It is move-only, and
its destructor and a move out of it overwrite those bytes with stores the compiler cannot remove. So a key and a
`secret_bytes` belong on the stack or in a `unique_ptr`; in a managed object they would stay in memory until the
collector's cycle found the object dead. Private keys read from PEM go the same way: `from_pem` takes bytes and
decodes their base64 straight into a `secret_bytes`, and a key file is read by `read_secret` and handed to
`from_pem` without passing through managed memory. So do [net::tls](../net/tls/README.md)'s identity (its key's PEM) and
the password of [compress::sevenzip](../compress/sevenzip-options.md).

The tests hold the rule. After a TLS exchange and a 7z archive read with a password, the managed pages are
searched: none holds the key, a traffic secret or the password. The exports and imports of every key take no
managed page over thousands of calls, and every block a `secret_bytes` frees is zero.

**Nonces.** An AEAD's nonce must never repeat under one key: a repeated nonce under AES-GCM gives away the
authentication key, and under both AEADs the XOR of the two plaintexts. Where the two sides count their messages,
a [nonce_counter](nonce_counter/README.md) makes the nonces and refuses to wrap. Random nonces are safe only with 24 bytes:
[xchacha20_poly1305](xchacha20_poly1305/README.md)'s `seal_random` draws one and writes it in front. Twelve random bytes
(AES-GCM, ChaCha20-Poly1305) are safe for about 2^32 messages under one key, as SP 800-38D says, and not beyond.

**Passwords.** [pbkdf2](pbkdf2/README.md) derives a key from a password (a file's encryption key, a protocol that names
it). It is not the way to store passwords: it costs an attacker's graphics card as little as it costs the server,
and a password hash for storage wants one that costs memory, Argon2id or scrypt, which come after version 1.

**Constant time.** Nothing that depends on a secret chooses a branch or an address: the AES S-box is the
processor's or computed bitsliced, never a table; the curves' ladders and table scans go through masks; the
inverses are Fermat's (RSA's one inverse, of its blinding factor, is taken of a product with a second random
number); RSA's private operation runs blinded and CRT over words of a width fixed by the key; tags are compared by
[constant_time::equal](constant_time/equal.md). What is public — a length, a public key, whether a verification
passed — may take its own time. The tests measure the claim for the curves and RSA with dudect, on this machine
only; the machine code of the rest has been read for branches and loads that depend on data.

### Two roads: the processor's instructions and plain C++

On arm64 the module runs on the processor's own instructions: SHA-1 and SHA-256 (`FEAT_SHA1`, `FEAT_SHA256`),
SHA-512 and SHA-3 (`FEAT_SHA512`, `FEAT_SHA3`), AES (`AESE`, `AESMC`) and GHASH (`PMULL`), and NEON for ChaCha20.
Everywhere else, and in a build with `SGCL_CRYPTO_PORTABLE` defined, every algorithm has a body in plain C++ that
gives the same bytes, in constant time too: AES bitsliced, GHASH on integer products — correct and constant-time,
and two orders of magnitude slower than the instructions. The tests run every vector on both roads
(`tests_crypto` and `tests_crypto_portable`). A program is built with one setting of the macro throughout.

No build flag is needed for the fast roads. Their bodies are compiled on every target with a target attribute, and
the processor is asked once, at the first call, which instructions it has (`sysctlbyname` on macOS, `getauxval` on
Linux, `cpuid` on x86-64): so a program built for plain `armv8-a` or plain x86-64 still uses the instructions where
the processor has them, and takes the portable road where it has not. Where the target itself promises the
instructions, as Apple's does, the question folds away at compile time. A cipher's road is chosen with its key,
from the processor alone.

| Algorithm | On arm64 | On x86-64 |
|---|---|---|
| AES (GCM, CTR, CBC decryption), GHASH | AESE/AESMC and PMULL, eight blocks at a time | AES-NI and PCLMULQDQ, eight blocks at a time |
| SHA-1, SHA-256 | the SHA1/SHA256 instructions | SHA-NI |
| SHA-512, SHA-3 | the SHA512 and SHA3 instructions | portable |
| ChaCha20 | NEON, four and eight blocks | SSE2, four blocks; AVX2, eight |
| Poly1305 | 64-bit multiplications | 64-bit multiplications |

On x86-64 the SHA-NI and AVX2 roads are compiled but not yet run on a processor that has them (the machines this is
built on emulate x86-64 without them); until they are, they are unverified, and the x86 machine closes them.
AES-NI, PCLMULQDQ and the SSE2 ChaCha20 pass every vector of the suite.

## Functions

| Function | Header | Description |
|---|---|---|
| [block_size](block_size.md) | `hash_id.h` | the block of the digest a `hash_id` names, in bytes: what HMAC pads its key to |
| [constant_time::equal](constant_time/equal.md) | `constant_time.h` | whether two runs of bytes are the same, every byte read whatever they hold: Go's `subtle.ConstantTimeCompare` |
| [crypto_category](crypto_category.md) | `error.h` | the `std::error_category` of the module's codes |
| [digest](digest.md) | `hash_id.h` | the digest of bytes by the algorithm a `hash_id` names |
| [digest_file, async_digest_file](digest_file.md) | `hash_id.h` | the digest of a whole file by the algorithm a `hash_id` names, read a block at a time |
| [digest_size](digest_size.md) | `hash_id.h` | the length of the digest a `hash_id` names, in bytes |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as a `std::error_code` of the module's category |
| [random::bytes](random/bytes.md) | `random.h` | random bytes that are not a secret: a salt, a nonce, an id; Go's `crypto/rand` |
| [random::fill](random/fill.md) | `random.h` | the program's buffer filled with random bytes |
| [random::secret](random/secret.md) | `random.h` | random bytes that are a secret, in a `secret_bytes`: a key, a seed |
| [read_secret](read_secret.md) | `read_secret.h` | a file's bytes as a secret, never through managed memory: a key's PEM, a password |
| [secure_zero](secure_zero.md) | `secure_zero.h` | zeros over a buffer that held a secret, which the compiler cannot drop |

## Classes

The namespaces of the public-key algorithms, each a page with its types and what they share, and the page on
ECDSA, which the private keys of both NIST curves sign by:

| Namespace | Header | Description |
|---|---|---|
| [constant_time](constant_time/README.md) | `constant_time.h` | comparison that does not tell how much of a tag was right |
| [ecdsa](ecdsa.md) | `ecdsa.h` | how the NIST curves sign and verify: hedged RFC 6979 nonces, DER and raw signatures |
| [ed25519](ed25519.md) | `ed25519.h` | signatures (RFC 8032): SSH keys, certificates, packages, JWTs (EdDSA); Go's `crypto/ed25519` |
| [mlkem512, mlkem768, mlkem1024](mlkem.md) | `mlkem.h` | ML-KEM (FIPS 203), post-quantum key encapsulation: TLS 1.3's X25519MLKEM768; Go's `crypto/mlkem` |
| [p256](p256.md) | `p256.h` | NIST P-256: ECDH and ECDSA of TLS, X.509, WebAuthn, JWTs (ES256); Go's `crypto/ecdh` and `crypto/ecdsa` |
| [p384](p384.md) | `p384.h` | NIST P-384: the same on the curve of CNSA (ES384) |
| [random](random/README.md) | `random.h` | random bytes from a generator in the process, seeded from the system |
| [rsa](rsa.md) | `rsa.h` | signatures of certificates and JWTs (PKCS #1 v1.5, PSS: RS256, PS256), OAEP key transport; Go's `crypto/rsa` |
| [x25519](x25519.md) | `x25519.h` | key agreement (RFC 7748): TLS 1.3, SSH, WireGuard, Signal; Go's `crypto/ecdh.X25519()` |
| [x509](x509.md) | `x509.h` | certificates (RFC 5280): read, pooled and verified as a chain with the host name; Go's `crypto/x509` |

### Digests and key derivation

| Class | Header | Description |
|---|---|---|
| [hkdf\<H\>](hkdf/README.md) | `hkdf.h` | keys from a secret (RFC 5869): TLS 1.3's key schedule, after ECDH; `hkdf_sha256`; Go's `crypto/hkdf` |
| [hmac\<H\>](hmac/README.md) | `hmac.h` | a tag under a key (RFC 2104): API signatures, cookies, JWTs (HS256); `hmac_sha256`, `hmac_sha512`; Go's `crypto/hmac` |
| [pbkdf2\<H\>](pbkdf2/README.md) | `pbkdf2.h` | a key from a password (RFC 8018), not for storing passwords; Go's `crypto/pbkdf2` |
| [sha1](sha1/README.md) | `sha1.h` | SHA-1: Git's object names, old protocols; broken for signatures |
| [sha256, sha224](sha256/README.md) | `sha256.h` | SHA-256: the digest of TLS, certificates, JWTs, Bitcoin; Go's `crypto/sha256` |
| [sha3_256, sha3_224, sha3_384, sha3_512](sha3_256/README.md) | `sha3.h` | SHA-3 (FIPS 202): Keccak, the other family; Go's `crypto/sha3` |
| [sha512, sha384, sha512_256](sha512/README.md) | `sha512.h` | the larger digests of SHA-2; Ed25519's; Go's `crypto/sha512` |
| [shake256, shake128](shake256/README.md) | `sha3.h` | SHAKE (FIPS 202): output of any length, read as a secret |

### Ciphers

| Class | Header | Description |
|---|---|---|
| [aes](aes/README.md) | `aes.h` | the AES block cipher alone, a block at a time: for modes that authenticate otherwise; Go's `crypto/aes` |
| [aes_ctr](aes_ctr/README.md) | `ctr.h` | AES in counter mode, unauthenticated; Go's `cipher.NewCTR` |
| [aes_gcm](aes_gcm/README.md) | `gcm.h` | the AEAD of TLS, on the processor's AES instructions; Go's `cipher.NewGCM` |
| [chacha20](chacha20/README.md) | `chacha20.h` | the ChaCha20 stream cipher, unauthenticated |
| [chacha20_poly1305](chacha20_poly1305/README.md) | `chacha20_poly1305.h` | the other AEAD of TLS (RFC 8439); Go's `chacha20poly1305` |
| [nonce_counter](nonce_counter/README.md) | `nonce_counter.h` | nonces that never repeat under one key |
| [xchacha20_poly1305](xchacha20_poly1305/README.md) | `chacha20_poly1305.h` | the AEAD whose 24-byte nonce may be random, `seal_random` |

### Keys

| Class | Header | Description |
|---|---|---|
| [ed25519::private_key](ed25519-private_key/README.md) | `ed25519.h` | signs; a seed of 32 bytes, zeroed when it goes |
| [ed25519::public_key](ed25519-public_key/README.md) | `ed25519.h` | verifies a signature |
| [mlkem768::decapsulation_key](mlkem768-decapsulation_key/README.md) | `mlkem.h` | the owner's key, its seed of 64 bytes; decapsulates a ciphertext to the shared key |
| [mlkem768::encapsulation](mlkem768-encapsulation.md) | `mlkem.h` | what an encapsulation gives: the shared key and the ciphertext |
| [mlkem768::encapsulation_key](mlkem768-encapsulation_key/README.md) | `mlkem.h` | the published key; encapsulates to it |
| [p256::ecdh_key](p256-ecdh_key/README.md) | `p256.h` | the private key of ECDH on P-256 (and P-384) |
| [p256::private_key](p256-private_key/README.md) | `p256.h` | the private key of ECDSA on P-256 (and P-384) |
| [p256::public_key](p256-public_key/README.md) | `p256.h` | a point: verifies a signature, the peer of an ECDH key |
| [rsa::private_key](rsa-private_key/README.md) | `rsa.h` | signs (PKCS #1 v1.5, PSS) and decrypts (OAEP); blinded, CRT |
| [rsa::public_key](rsa-public_key/README.md) | `rsa.h` | verifies and encrypts |
| [x25519::private_key](x25519-private_key/README.md) | `x25519.h` | a scalar of 32 bytes; the shared secret with a peer |
| [x25519::public_key](x25519-public_key/README.md) | `x25519.h` | a point of 32 bytes, the peer of a private key |

### Certificates

| Class | Header | Description |
|---|---|---|
| [x509::certificate](x509-certificate/README.md) | `x509.h` | a certificate from DER or PEM, its names, key and extensions; `verify` of its chain |
| [x509::certificate_pool](x509-certificate_pool/README.md) | `x509.h` | a set of certificates: the roots of a verification, the system's own |
| [x509::extension](x509-extension.md) | `x509.h` | an extension as the certificate has it: its OID, whether it is critical, its value |
| [x509::ip_address](x509-ip_address/README.md) | `x509.h` | an IP address of a certificate's subject alternative names |
| [x509::ip_range](x509-ip_range/README.md) | `x509.h` | an IP range of a CA's name constraints: an address and a mask |
| [x509::name](x509-name/README.md) | `x509.h` | a distinguished name, the issuer or the subject, and its text |
| [x509::name::attribute](x509-name-attribute.md) | `x509.h` | one attribute of a name: its type and its value |
| [x509::public_key](x509-public_key/README.md) | `x509.h` | a certificate's public key as one of the module's keys |
| [x509::verify_options](x509-verify_options.md) | `x509.h` | what a verification asks: the roots, the intermediates, the name, the time, the key usages |

### Secrets and errors

| Class | Header | Description |
|---|---|---|
| [error](error/README.md) | `error.h` | the one error of the module: `code()`, `offset()`, `reason()`, `message()` |
| [secret\<N\>](secret/README.md) | `secret.h` | N secret bytes held in the object: a shared secret, a scalar; move-only, zeroed when it goes |
| [secret_bytes](secret_bytes/README.md) | `secret.h` | secret bytes of a length known when the program runs, never in managed memory |

### Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the codes of the module's errors |
| [hash_id](hash_id.md) | `hash_id.h` | a digest named by data: a certificate's algorithm, a parameter of RSA and ECDSA; Go's `crypto.Hash` |
| [x509::ext_key_usage](x509-ext_key_usage.md) | `x509.h` | a purpose of extKeyUsage the module knows by name: what a leaf may be used for |
| [x509::key_kind](x509-key_kind.md) | `x509.h` | the algorithm of a certificate's public key |
| [x509::key_usage](x509-key_usage.md) | `x509.h` | the bits of a certificate's keyUsage, as flags |
| [x509::reason](x509-reason.md) | `error.h` | why a certificate chain does not verify |
| [x509::signature_algorithm](x509-signature_algorithm.md) | `x509.h` | the algorithm a certificate is signed with |

## Mixins

| Mixin | Header | Description |
|---|---|---|
| [aead](mixin/aead/README.md) | `mixin/aead.h` | the interface the AEADs share, Go's `cipher.AEAD`: `seal`, `open`, `seal_to`, `open_to` |

## See also

- [Benchmarks](benchmarks.md): the module against OpenSSL and Go
- [hash](../hash/README.md): the hashers that are not cryptographic, and the shape the digests take
- [net::tls](../net/tls/README.md): TLS 1.3 on this module
- [math::big_integer](../math/big_integer/README.md): numbers that are public
- [The modules](../README.md)
