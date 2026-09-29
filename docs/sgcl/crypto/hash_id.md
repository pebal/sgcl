# sgcl::crypto::hash_id, sgcl::crypto::digest

```cpp
#include "sgcl/crypto/hash_id.h"   // or "sgcl/crypto/crypto.h"

namespace sgcl::crypto {
    enum class hash_id : uint8_t {
        sha1 = 1, sha224, sha256, sha384, sha512, sha512_256,
        sha3_224, sha3_256, sha3_384, sha3_512
    };
    size_t digest_size(hash_id id);
    size_t block_size(hash_id id);
    vector<byte> digest(hash_id id, const slice<const byte>& data);   // bytes or text
}
```

A digest named by a value rather than by a type, Go's `crypto.Hash`: for where the algorithm is known only at run time — from a certificate's signature algorithm, from a TLS handshake, from a key's parameters — and a template cannot be chosen. RSA-PSS, OAEP and ECDSA over a digest the data names take one, as `sign_digest(hash_id::sha256, d)`. Where the digest is known in the code, its type is simpler and costs nothing: `sha256::of(data)`, `hmac<sha256>`.

**The implementation has not been through an independent cryptographic audit.**

## Rules

- **`digest(id, data)`** is `T::of(data)` for the type `id` names, as a `vector<byte>` of `digest_size(id)` bytes.
- **`digest_size` and `block_size`** are the type's constants: 32 and 64 for `hash_id::sha256`, 32 and 136 (the rate) for `hash_id::sha3_256`.
- **A value outside the list** (a number cast to `hash_id`) is a broken contract: `std::invalid_argument`. A protocol that reads an algorithm from data maps its own identifiers (an OID, a TLS code point) to `hash_id` and reports an unknown one as `errc::unsupported`.
- SHAKE is not here: it has no fixed size.

## Example

```cpp
#include "sgcl/crypto/crypto.h"
#include "sgcl/encoding/encoding.h"
#include "sgcl/io/io.h"

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

[`sha256`](sha256.md), [`sha512`](sha512.md), [`sha3`](sha3.md), [`sha1`](sha1.md).
