[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [public_key](README.md)

# sgcl::crypto::mldsa65::public_key::from_bytes

```cpp
static expected<public_key, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

The public key of its bytes (pkEncode, FIPS 204 §7.2: ρ and t1), as [bytes](bytes.md) gives them and Go's `mldsa.NewPublicKey` takes them; the matrix Â and t1 transformed are made once, here.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | `public_key_size` bytes (1312, 1952, 2592 for the three sets) |

## Return value

The key, or `errc::invalid_key` for another length. Every string of the right length is a key.

## Complexity

Constant: one sampling of Â.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    auto published = key.public_key().bytes();
    auto read = crypto::mldsa65::public_key::from_bytes(published);
    println("{}", read->verify("m", key.sign("m")));
    println("{}", bool(crypto::mldsa65::public_key::from_bytes(vector<byte>(100))));
}
```

Output:

```text
true
false
```

## See also

- [bytes](bytes.md)
- [sgcl::crypto::mldsa65::public_key](README.md)
