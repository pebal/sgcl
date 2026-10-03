[sgcl](../../README.md) › [crypto](../README.md) › [pbkdf2](README.md)

# sgcl::crypto::pbkdf2\<H\>::derive

```cpp
static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt,
                           uint32_t iterations, size_t n);
```

`n` bytes derived from `password` and `salt` in `iterations` rounds of HMAC over `H` per block, as RFC 8018 §5.2 has
it, what Go's `pbkdf2.Key` gives. The password is the HMAC's key, bytes or text as given; the arguments are checked
before anything is computed. The keyed states and every intermediate block are zeroed before the call returns.

The output is a [secret_bytes](../secret_bytes/README.md): up to 64 bytes in the object itself, past that in plain memory
zeroed when it goes, never in managed memory.

## Parameters

| Parameter | Description |
|---|---|
| `password` | the password, bytes or text |
| `salt` | the salt, random and stored beside the result |
| `iterations` | the rounds per block, at least 1: the work factor |
| `n` | the number of bytes, at most 2^32 − 1 blocks of `H::digest_size` |

## Return value

The `n` bytes.

## Complexity

`iterations` HMACs for every `H::digest_size` bytes of `n`.

## Exceptions

`std::invalid_argument` when `iterations` is 0, or when `n` is more than 2^32 − 1 blocks of the digest's size.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 6070, HMAC-SHA-1
    for (uint32_t iterations : {1, 2, 4096}) {
        auto key = crypto::pbkdf2<crypto::sha1>::derive("password", "salt", iterations, 20);
        println(encoding::hex::encode(key));
    }
    // RFC 7914 §11, HMAC-SHA-256
    println(encoding::hex::encode(crypto::pbkdf2<crypto::sha256>::derive("passwd", "salt", 1, 64)));

    try {
        crypto::pbkdf2<crypto::sha256>::derive("password", "salt", 0, 32);
    } catch (const std::invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
0c60c80f961f0e71f3a9b524af6012062fe037a6
ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957
4b007901b765489abead49d926f721d065a429c1
55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783
sgcl::crypto::pbkdf2: no iterations
```

## See also

- [derive_to](derive_to.md): into a buffer of the caller's
- [hmac](../hmac/README.md): the function of each round
- [sgcl::crypto::pbkdf2\<H\>](README.md)
