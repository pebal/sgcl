[sgcl](../../README.md) › [crypto](../README.md) › [random](../random.md)

# sgcl::crypto::random::secret

```cpp
secret_bytes secret(size_t n) noexcept;
```

Returns `n` random bytes that are a secret, a key or a seed, in a [secret_bytes](../secret_bytes.md): up to 64 bytes
in the object itself, past that in plain memory, never in managed memory, and zeroed when it goes.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes |

## Return value

A secret of `n` random bytes.

## Complexity

Linear in `n`.

## Exceptions

None. A system that gives no random bytes ends the program, with a line on stderr.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes key = crypto::random::secret(32);
    crypto::aes_gcm aead(key);

    // RFC 8439's nonce; a nonce is not a secret
    auto nonce = encoding::hex::decode("070000004041424344454647");
    auto sealed = aead.seal(nonce, "a message");
    println("{} {}", key.size(), aead.open(nonce, sealed).has_value());
}
```

Output:

```text
32 true
```

## See also

- [bytes](bytes.md): random bytes for what is not a secret
- [secret_bytes](../secret_bytes.md): the secret it returns
- [sgcl::crypto::random](../random.md)
