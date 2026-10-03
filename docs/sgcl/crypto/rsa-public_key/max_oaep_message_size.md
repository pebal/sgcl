[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [public_key](README.md)

# sgcl::crypto::rsa::public_key::max_oaep_message_size

```cpp
size_t max_oaep_message_size(hash_id id) const;
```

Returns the longest message [encrypt_oaep](encrypt_oaep.md) takes under `id`: the modulus's bytes less twice the
digest's less 2 (RFC 8017 §7.1.1), 190 for a 2048-bit key and SHA-256. It is also the room
[decrypt_oaep_to](../rsa-private_key/decrypt_oaep_to.md) asks of its buffer.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the hash OAEP is used with |

## Return value

`size() - 2 * digest_size(id) - 2`, or 0 when the key is too small for the hash at all.

## Complexity

Constant.

## Exceptions

- `logic_error` when the key was moved from.
- `invalid_argument` when `id` is not a [hash_id](../hash_id.md) the module has.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto pem = crypto::read_secret("tests/net/tls_testdata/rsa.key");  // the tree's test key
    crypto::rsa::private_key key = crypto::rsa::private_key::from_pem(pem);

    crypto::rsa::public_key pub = key.public_key();
    for (auto id : {crypto::hash_id::sha1, crypto::hash_id::sha256, crypto::hash_id::sha512}) {
        println("{}", pub.max_oaep_message_size(id));
    }
}
```

Output:

```text
214
190
126
```

## See also

- [encrypt_oaep](encrypt_oaep.md)
- [decrypt_oaep_to](../rsa-private_key/decrypt_oaep_to.md)
- [sgcl::crypto::rsa::public_key](README.md)
