[sgcl](../../README.md) › [crypto](../README.md) › [random](../random.md)

# sgcl::crypto::random::bytes

```cpp
vector<byte> bytes(size_t n);
```

Returns `n` random bytes in a new `vector<byte>`, a managed buffer: for what is not a secret, a salt, a nonce, an id,
a token that is public anyway. The collector frees a managed buffer without zeroing it, so a key comes from
[secret](secret.md) instead.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number of bytes |

## Return value

A vector of `n` random bytes.

## Complexity

Linear in `n`.

## Exceptions

`length_error` when `n` is above the `max_size()` of the vector.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto salt = crypto::random::bytes(16);
    println("{} {}", salt.size(), encoding::hex::encode(salt));
    println("{}", encoding::base64::raw_url.encode(crypto::random::bytes(24)));
}
```

Sample output:

```text
16 3dddc304ed777539c94a5689973721ce
AuJ6OzDeDXgVqj-T4Y7bzG2itUszKCMY
```

## See also

- [secret](secret.md): random bytes for a key
- [fill](fill.md): random bytes into a buffer of the program's
- [sgcl::crypto::random](../random.md)
