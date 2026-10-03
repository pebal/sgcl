[sgcl](../../README.md) › [crypto](../README.md) › [chacha20](README.md)

# sgcl::crypto::chacha20::from_key

```cpp
static expected<chacha20, error> from_key(const slice<const byte>& key,
                                          const slice<const byte>& nonce);
```

Sets up a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](chacha20.md) throws. The nonce is the program's to make, and one
of the wrong length still throws, as with [aes_ctr::from_key](../aes_ctr/from_key.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key's bytes: 32 of them make a key |
| `nonce` | 12 bytes, or 24 for XChaCha20 |

## Return value

The object, or an [error](../error/README.md) of [errc::invalid_key](../errc.md) whose message names the length when
`key` is not 32 bytes.

## Complexity

Constant.

## Exceptions

`invalid_argument` when `key` is a key and `nonce` is not 12 or 24 bytes.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> stored(16);  // a key as a file would hold it, of AES's length
    vector<byte> nonce(12);
    auto broken = crypto::chacha20::from_key(stored, nonce);
    println("{} {}", broken.error().code() == crypto::errc::invalid_key, broken.error().message());

    stored.resize(32);
    println("{}", crypto::chacha20::from_key(stored, nonce).has_value());
    try {
        auto cipher = crypto::chacha20::from_key(stored, nonce.as_slice(0, 8));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
true chacha20: a key of 16 bytes
true
sgcl::crypto::chacha20: a nonce of 8 bytes, not 12 or 24
```

## See also

- [(constructor)](chacha20.md): a key whose length the program fixes
- [error](../error/README.md): the error of the module
- [sgcl::crypto::chacha20](README.md)
