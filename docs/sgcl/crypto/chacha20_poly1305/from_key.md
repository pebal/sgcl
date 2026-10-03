[sgcl](../../README.md) › [crypto](../README.md) › [chacha20_poly1305](../chacha20_poly1305.md)

# sgcl::crypto::chacha20_poly1305::from_key

```cpp
static expected<chacha20_poly1305, error> from_key(const slice<const byte>& key) noexcept;
```

Takes a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](chacha20_poly1305.md) throws.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key's bytes: 32 of them make a key |

## Return value

The object, or an [error](../error.md) of [errc::invalid_key](../errc.md) whose message names the length when
`key` is not 32 bytes.

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
    // A key as a file would hold it, cut short
    vector<byte> stored =
        encoding::hex::decode("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e");
    auto broken = crypto::chacha20_poly1305::from_key(stored);
    println("{} {}", broken.error().code() == crypto::errc::invalid_key, broken.error().message());

    stored.push_back(byte(0x9f));
    auto aead = crypto::chacha20_poly1305::from_key(stored);
    println("{}", aead.has_value());
}
```

Output:

```text
true chacha20_poly1305: a key of 31 bytes
true
```

## See also

- [(constructor)](chacha20_poly1305.md): a key whose length the program fixes
- [error](../error.md): the error of the module
- [sgcl::crypto::chacha20_poly1305](../chacha20_poly1305.md)
