[sgcl](../../README.md) › [crypto](../README.md) › [aes](../aes.md)

# sgcl::crypto::aes::from_key

```cpp
static expected<aes, error> from_key(const slice<const byte>& key) noexcept;
```

Sets up a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](aes.md) throws.

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
    // FIPS 197, appendix B: its key, and a key as a file would hold it, too long
    vector<byte> stored = encoding::hex::decode("2b7e151628aed2a6abf7158809cf4f3c00");
    auto broken = crypto::aes::from_key(stored);
    println("{} {}", broken.error().code() == crypto::errc::invalid_key, broken.error().message());

    stored.pop_back();
    auto cipher = crypto::aes::from_key(stored);
    println("{}", cipher->key_size());
}
```

Output:

```text
true aes: a key of 17 bytes
16
```

## See also

- [(constructor)](aes.md): a key whose length the program fixes
- [error](../error.md): the error of the module
- [sgcl::crypto::aes](../aes.md)
