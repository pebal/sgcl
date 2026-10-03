[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::hash_id

```cpp
#include "sgcl/crypto/hash_id.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    enum class hash_id : uint8_t {
        sha1 = 1,
        sha224,
        sha256,
        sha384,
        sha512,
        sha512_256,
        sha3_224,
        sha3_256,
        sha3_384,
        sha3_512
    };

    size_t digest_size(hash_id id);
    size_t block_size(hash_id id);
    vector<byte> digest(hash_id id, const slice<const byte>& data);
    expected<vector<byte>, io::error> digest_file(hash_id id, const string& path);
    async::task<expected<vector<byte>, io::error>> async_digest_file(hash_id id,
                                                                    const string& path) noexcept;
}
```

`sgcl::crypto::hash_id` is a digest named by a value rather than by a type, Go's `crypto.Hash`: for where the
algorithm is known only at run time — from a certificate's signature algorithm, from a TLS handshake, from a key's
parameters — and a template cannot be chosen. RSA-PSS, OAEP and ECDSA over a digest the data names take one, as
`sign_digest(hash_id::sha256, d)` ([rsa](rsa.md), [ecdsa](ecdsa.md)). Where the digest is known in the code, its type
is simpler and costs nothing: `sha256::of(data)`, `hmac<sha256>`.

Four functions answer for an id what the type answers by its members: [digest_size](digest_size.md) and
[block_size](block_size.md), the type's constants, [digest](digest.md), the type's one-shot `of(data)`, and
[digest_file](digest_file.md), its `of_file(path)`, each digest as a `vector<byte>`. No hasher is made to answer the
first two.

**The implementation has not been through an independent cryptographic audit.**

| Value | Description |
|---|---|
| `sha1` | SHA-1, 20 bytes, block 64 ([sha1](sha1/README.md)); the first value, 1 |
| `sha224` | SHA-224, 28 bytes, block 64 ([sha256](sha256/README.md)) |
| `sha256` | SHA-256, 32 bytes, block 64 ([sha256](sha256/README.md)) |
| `sha384` | SHA-384, 48 bytes, block 128 ([sha512](sha512/README.md)) |
| `sha512` | SHA-512, 64 bytes, block 128 ([sha512](sha512/README.md)) |
| `sha512_256` | SHA-512/256, 32 bytes, block 128 ([sha512](sha512/README.md)) |
| `sha3_224` | SHA3-224, 28 bytes, rate 144 ([sha3_256](sha3_256/README.md)) |
| `sha3_256` | SHA3-256, 32 bytes, rate 136 ([sha3_256](sha3_256/README.md)) |
| `sha3_384` | SHA3-384, 48 bytes, rate 104 ([sha3_256](sha3_256/README.md)) |
| `sha3_512` | SHA3-512, 64 bytes, rate 72 ([sha3_256](sha3_256/README.md)); the last value, 10 |

## Rules

- **`digest(id, data)`** is `T::of(data)` for the type `T` that `id` names, as a `vector<byte>` of `digest_size(id)`
  bytes; **`digest_file(id, path)`** is `T::of_file(path)` the same way, or the error of the file.
- **`digest_size` and `block_size`** are the type's constants: 32 and 64 for `hash_id::sha256`, 32 and 136 (the
  rate) for `hash_id::sha3_256`.
- **A value outside the list** (a number cast to `hash_id`, 0 among them) is a broken contract: each of the four
  throws `std::invalid_argument` (`async_digest_file` out of the `co_await` of its task). A protocol that reads an
  algorithm from data maps its own identifiers (an OID, a TLS code point) to `hash_id` and reports an unknown one as
  `errc::unsupported` ([errc](errc.md)).
- **SHAKE is not here**: it has no fixed size ([shake256](shake256/README.md)).

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (auto id : {crypto::hash_id::sha256, crypto::hash_id::sha3_256}) {
        println("{} {}", digest_size(id), encoding::hex::encode(digest(id, "abc")));
    }
}
```

Output:

```text
32 ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
32 3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532
```

## See also

- [digest_size](digest_size.md), [block_size](block_size.md), [digest](digest.md), [digest_file](digest_file.md): what
  an id answers
- [sha1](sha1/README.md), [sha256](sha256/README.md), [sha512](sha512/README.md), [sha3_256](sha3_256/README.md): the digests it names
- [rsa](rsa.md), [ecdsa](ecdsa.md), [x509](x509.md): where a digest is named by data
- [The module](README.md)
