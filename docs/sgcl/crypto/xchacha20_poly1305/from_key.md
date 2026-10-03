[sgcl](../../README.md) › [crypto](../README.md) › [xchacha20_poly1305](../xchacha20_poly1305.md)

# sgcl::crypto::xchacha20_poly1305::from_key

```cpp
static expected<xchacha20_poly1305, error> from_key(const slice<const byte>& key) noexcept;
```

Takes a key that came with data, a file or a message, whose length the program cannot vouch for: a key of the
wrong length is an error, where the [constructor](xchacha20_poly1305.md) throws.

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
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> stored(33);  // a key as a file would hold it, a byte too long
    auto broken = crypto::xchacha20_poly1305::from_key(stored);
    println("{} {}", broken.error().code() == crypto::errc::invalid_key, broken.error().message());

    stored.pop_back();
    auto aead = crypto::xchacha20_poly1305::from_key(stored);
    println("{}", aead.has_value());
}
```

Output:

```text
true xchacha20_poly1305: a key of 33 bytes
true
```

## See also

- [(constructor)](xchacha20_poly1305.md): a key whose length the program fixes
- [error](../error.md): the error of the module
- [sgcl::crypto::xchacha20_poly1305](../xchacha20_poly1305.md)
