# sgcl::crypto::hkdf

```cpp
#include "sgcl/crypto/hkdf.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    template<class H> class hkdf;        // HKDF (RFC 5869) over a digest of the module
    class hkdf<H>::prk;                  // a pseudorandom key: extract()'s result, a secret
    using hkdf_sha256 = hkdf<sha256>;
}
```

HKDF, Go's `crypto/hkdf`: keys made from a secret that is not yet one — the output of a key exchange, a master key, a password already stretched — in two steps. `extract` concentrates the input's entropy into a pseudorandom key (PRK) of the digest's size, under a salt; `expand` makes from the PRK as many bytes as asked, bound to an `info` that names what they are for, so that one PRK gives independent keys for independent uses (a key each way, a key and an IV). `derive` is both at once. TLS 1.3's key schedule, Signal, Noise, HPKE and WireGuard are built on it. Written from RFC 5869, tested against its vectors and against OpenSSL on random salts, inputs, infos and output sizes up to the maximum.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **The salt** is not a secret: a random value both sides know, or a fixed one naming the protocol. An empty salt is the digest's size of zeros (RFC 5869 §2.2). A salt makes the PRK independent of other uses of the same input keying material.
- **`info`** binds the output to its use: two calls with different infos give unrelated keys. It may be empty.
- **At most `max_size`** bytes from one PRK: 255 blocks of the digest (8160 bytes for SHA-256). Past it `std::invalid_argument`, a broken contract.
- **The PRK is a secret**, held by `hkdf<H>::prk`: its bytes in the object, move-only, `clone()` for a copy meant, zeroed when it dies and when moved from. `bytes()` gives them where they lie, valid while the object lives, for a protocol that feeds one secret into the next (TLS 1.3): `expand` also takes a PRK as plain bytes.
- **What `expand` and `derive` give is a `vector<byte>`**, a managed buffer that stays in memory until the collector reuses its space. For a key that must not linger, `expand_to` and `derive_to` write into a buffer of the caller's — a stack array, a key object's own storage — and [`secure_zero`](secure_zero.md) clears it when done.
- **Not for passwords**: HKDF assumes its input already has the entropy. A password goes through [`pbkdf2`](pbkdf2.md) (or Argon2id after version 1) first.

## Members

```cpp
static constexpr size_t max_size = 255 * H::digest_size;

static prk extract(const slice<const byte>& salt, const slice<const byte>& ikm) noexcept;
static vector<byte> expand(const prk& key, const slice<const byte>& info, size_t n);
static vector<byte> expand(const slice<const byte>& key, const slice<const byte>& info, size_t n);
static void expand_to(const slice<byte>& out, const prk& key, const slice<const byte>& info);
static void expand_to(const slice<byte>& out, const slice<const byte>& key, const slice<const byte>& info);
static vector<byte> derive(const slice<const byte>& salt, const slice<const byte>& ikm, const slice<const byte>& info, size_t n);
static void derive_to(const slice<byte>& out, const slice<const byte>& salt, const slice<const byte>& ikm, const slice<const byte>& info);

class prk {
public:
    static constexpr size_t size = H::digest_size;
    prk(prk&& other) noexcept;                    // other zeroed
    prk& operator=(prk&& other) noexcept;
    prk(const prk&) = delete;
    ~prk();                                       // zeroed
    prk clone() const noexcept;
    slice<const byte> bytes() const noexcept;     // the key where it lies in the object
};
```

Every `slice<const byte>` takes bytes or text: a `vector<byte>`, a `string`, a literal (to its first NUL), a `std::string_view`, a `uint8_t` array.

## Example

```cpp
#include "sgcl/crypto/hkdf.h"
#include "sgcl/encoding/hex.h"
#include "sgcl/io/print.h"

using namespace sgcl;

int main() {
    // the secret a key exchange gave both sides
    auto shared_secret = encoding::hex::decode("4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
    auto prk = crypto::hkdf_sha256::extract("my protocol v1", shared_secret);
    auto client_key = crypto::hkdf_sha256::expand(prk, "client to server", 32);
    auto server_key = crypto::hkdf_sha256::expand(prk, "server to client", 32);
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

[`hmac`](hmac.md), which it is made of; [`pbkdf2`](pbkdf2.md) for passwords; [`secure_zero`](secure_zero.md).
