[sgcl](../../README.md) › [crypto](../README.md) › [aes_ctr](../aes_ctr.md)

# sgcl::crypto::aes_ctr::from_key

```cpp
static expected<aes_ctr, error> from_key(const slice<const byte>& key, const slice<const byte>& iv);
```

Sets up a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](aes_ctr.md) throws. The initial counter is the program's to make,
and one of the wrong length still throws.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key's bytes: 16, 24 or 32 of them make a key |
| `iv` | the initial counter block, 16 bytes |

## Return value

The object, or an [error](../error.md) of [errc::invalid_key](../errc.md) whose message names the length when
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
    vector<byte> stored(15);  // a key as a file would hold it, a byte short
    vector<byte> iv(16);
    auto broken = crypto::aes_ctr::from_key(stored, iv);
    println("{} {}", broken.error().code() == crypto::errc::invalid_key, broken.error().message());

    stored.push_back(byte(0));
    println("{}", crypto::aes_ctr::from_key(stored, iv)->key_size());
    try {
        auto ctr = crypto::aes_ctr::from_key(stored, iv.as_slice(0, 8));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true aes_ctr: a key of 15 bytes
16
sgcl::crypto::aes_ctr: an initial counter of 8 bytes, not 16
```

## See also

- [(constructor)](aes_ctr.md): a key whose length the program fixes
- [error](../error.md): the error of the module
- [sgcl::crypto::aes_ctr](../aes_ctr.md)
