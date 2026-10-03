[sgcl](../../README.md) › [crypto](../README.md)

# sgcl::crypto::hkdf\<H\>

```cpp
#include "sgcl/crypto/hkdf.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    template<class H>
    class hkdf;

    using hkdf_sha256 = hkdf<sha256>;
}
```

`sgcl::crypto::hkdf<H>` is HKDF of RFC 5869, Go's `crypto/hkdf`: keys made from a secret that is not yet one — the
output of a key exchange, a master key, a password already stretched — in two steps. [extract](extract.md)
concentrates the input's entropy into a pseudorandom key (PRK) of the digest's size, under a salt;
[expand](expand.md) makes from the PRK as many bytes as asked, bound to an `info` that names what they are for,
so that one PRK gives independent keys for independent uses (a key each way, a key and an IV).
[derive](derive.md) is both at once. TLS 1.3's key schedule, Signal, Noise, HPKE and WireGuard are built on it.
The digest is a parameter of the template, as for [hmac](../hmac/README.md), of which HKDF is made; `hkdf_sha256` names the
most used. Written from RFC 5869, tested against its vectors and against OpenSSL on random salts, inputs, infos and
output sizes up to the maximum.

The class holds nothing: its functions are static, and the PRK between the two steps is an object of its own,
[hkdf\<H\>::prk](../hkdf-prk/README.md), a secret that zeroes itself.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The salt** is not a secret: a random value both sides know, or a fixed one naming the protocol. An empty salt is
  the digest's size of zeros (RFC 5869 §2.2). A salt makes the PRK independent of other uses of the same input keying
  material.
- **`info`** binds the output to its use: two calls with different infos give unrelated keys. It may be empty.
- **At most `max_size`** bytes from one PRK: 255 blocks of the digest (8160 bytes for SHA-256). Past it
  `std::invalid_argument`, a broken contract.
- **The PRK is a secret**, held by [hkdf\<H\>::prk](../hkdf-prk/README.md): its bytes in the object, move-only, `clone()` for a
  copy meant, zeroed when it dies and when moved from. `bytes()` gives them where they lie, valid while the object
  lives, for a protocol that feeds one secret into the next (TLS 1.3): `expand` also takes a PRK as plain bytes.
- **What `expand` and `derive` give is a [secret_bytes](../secret_bytes/README.md)**: up to 64 bytes in the object itself (no
  allocation), past that in plain memory zeroed when it goes, never in managed memory. `expand_to` and `derive_to`
  write into a buffer of the caller's instead: a stack array, a key object's own storage. The intermediate blocks are
  zeroed before every call returns.
- **Every `slice<const byte>`** takes bytes or text: a `vector<byte>`, a `string`, a literal (to its first NUL), a
  `std::string_view`, a `uint8_t` array.
- **Not for passwords**: HKDF assumes its input already has the entropy. A password goes through [pbkdf2](../pbkdf2/README.md)
  (or Argon2id after version 1) first.

## Template parameters

| Parameter | Description |
|---|---|
| `H` | the digest of the HMAC: `sha1`, `sha224`, `sha256`, `sha384`, `sha512`, `sha512_256`, `sha3_224`, `sha3_256`, `sha3_384` or `sha3_512`. Any other type is rejected at compile time. |

## Member types

| Type | Definition |
|---|---|
| [prk](../hkdf-prk/README.md) | a pseudorandom key: what `extract` gives and `expand` takes, a secret |

## Member objects

| Constant | Value | Description |
|---|---|---|
| `max_size` | `255 * H::digest_size` | the most `expand` makes from one PRK: 8160 bytes for SHA-256, `static constexpr size_t` |

## Member functions

| Function | Description |
|---|---|
| [extract](extract.md) | the PRK of input keying material under a salt (static) |
| [expand](expand.md) | bytes of output from a PRK, bound to an info (static) |
| [expand_to](expand_to.md) | `expand` into a buffer of the caller's (static) |
| [derive](derive.md) | `extract` and `expand` in one (static) |
| [derive_to](derive_to.md) | `derive` into a buffer of the caller's (static) |

## Complexity

`extract` is one HMAC over the input keying material; `expand` one HMAC of a block for every `H::digest_size` bytes
of output.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the secret a key exchange gave both sides
    auto shared_secret = encoding::hex::decode(
        "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
    auto extracted = crypto::hkdf_sha256::extract("my protocol v1", shared_secret);
    auto client_key = crypto::hkdf_sha256::expand(extracted, "client to server", 32);
    auto server_key = crypto::hkdf_sha256::expand(extracted, "server to client", 32);
    println(encoding::hex::encode(client_key));
    println(encoding::hex::encode(server_key));
}
```

Output:

```text
0b91bb7537e164ebd0ee15f7b5d3350019e308dc2c8117b4ecb7cc7e71fe24fe
13a9ea91f0fa8ee3d9c88dff7b46a95cb8ed9b2752f9a99986a574d98df57f1b
```

## See also

- [hkdf\<H\>::prk](../hkdf-prk/README.md): the pseudorandom key
- [hmac](../hmac/README.md): what it is made of
- [pbkdf2](../pbkdf2/README.md): keys from a password
- [secret_bytes](../secret_bytes/README.md), [secure_zero](../secure_zero.md): where the keys go
- [The module](../README.md)
