[sgcl](../../README.md) › [crypto](../README.md) › [aes_gcm](../aes_gcm.md)

# sgcl::crypto::aes_gcm::from_key

```cpp
static expected<aes_gcm, error> from_key(const slice<const byte>& key) noexcept;
```

Sets up a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](aes_gcm.md) throws.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key's bytes: 16, 24 or 32 of them make a key |

## Return value

The object, or an [error](../error.md) of [errc::invalid_key](../errc.md) whose message names the length when
`key` is not 16, 24 or 32 bytes.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // A key as a file would hold it, one byte short
    vector<byte> stored = encoding::hex::decode("feffe9928665731c6d6a8f94673083");
    auto broken = crypto::aes_gcm::from_key(stored);
    println("{} {}", broken.error().code() == crypto::errc::invalid_key, broken.error().message());

    stored.push_back(byte(0x08));
    auto gcm = crypto::aes_gcm::from_key(stored);
    println("{}", gcm->key_size());
}
```

Output:

```text
true aes_gcm: a key of 15 bytes
16
```

## See also

- [(constructor)](aes_gcm.md): a key whose length the program fixes
- [error](../error.md): the error of the module
- [sgcl::crypto::aes_gcm](../aes_gcm.md)
