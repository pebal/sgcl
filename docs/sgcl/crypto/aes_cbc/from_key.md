[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::from_key

```cpp
static expected<aes_cbc, error> from_key(const slice<const byte>& key, const slice<const byte>& iv);
```

Sets up a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](aes_cbc.md) throws. The IV is the program's to make, and one of the
wrong length still throws.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key's bytes: 16, 24 or 32 of them make a key |
| `iv` | the IV, 16 bytes |

## Return value

The object, or an [error](../error/README.md) of [errc::invalid_key](../errc.md) whose message names the length when
`key` is not 16, 24 or 32 bytes.

## Complexity

Constant.

## Exceptions

`invalid_argument` when `key` is a key and `iv` is not 16 bytes.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> stored(20), iv(16);
    auto broken = crypto::aes_cbc::from_key(stored, iv);
    println("{}", broken.error().message());
    stored.resize(24);
    println("{}", crypto::aes_cbc::from_key(stored, iv)->key_size());
}
```

Output:

```text
aes_cbc: a key of 20 bytes
24
```

## See also

- [(constructor)](aes_cbc.md): a key whose length the program fixes
- [sgcl::crypto::aes_cbc](README.md)
