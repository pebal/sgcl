[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::from_key

```cpp
static expected<aes_kw, error> from_key(const slice<const byte>& kek) noexcept;
```

Sets up a key-encryption key that came with data, whose length the program cannot vouch for: a key of the wrong
length is an error, where the [constructor](aes_kw.md) throws.

## Parameters

| Parameter | Description |
|---|---|
| `kek` | the key's bytes: 16, 24 or 32 of them make a key |

## Return value

The object, or an [error](../error/README.md) of [errc::invalid_key](../errc.md) whose message names the length.

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
    println("{}", crypto::aes_kw::from_key(vector<byte>(17)).error().message());
    println("{}", crypto::aes_kw::from_key(vector<byte>(16))->key_size());
}
```

Output:

```text
aes_kw: a key of 17 bytes
16
```

## See also

- [(constructor)](aes_kw.md): a key whose length the program fixes
- [sgcl::crypto::aes_kw](README.md)
