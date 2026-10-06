[sgcl](../../README.md) › [crypto](../README.md) › [aes_kw](README.md)

# sgcl::crypto::aes_kw::unwrap_padded

```cpp
[[nodiscard]] expected<secret_bytes, error> unwrap_padded(const slice<const byte>& wrapped) const;
```

Unwraps a key [wrap_padded](wrap_padded.md) wrapped and takes its padding off. Three things are checked, all of them
whatever the first gives and with no branch on the key: the register's constant, the length it holds against the
wrapped key's, and the padding's zeros. Any of them wrong is the same error.

## Parameters

| Parameter | Description |
|---|---|
| `wrapped` | the wrapped key, 16 bytes or more and a multiple of 8 |

## Return value

The key, as long as it was, as a [secret_bytes](../secret_bytes/README.md); or an [error](../error/README.md):
`errc::malformed` for a length that cannot be a wrapped key, `errc::authentication` when the check does not come out.
`[[nodiscard]]`: an unwrap whose result is dropped hides the check.

## Complexity

Linear in `wrapped.size()`.

## Exceptions

`logic_error` when the object was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::aes_kw wrapper(encoding::hex::decode("5840df6e29b02af1ab493b705bf16ea1ae8338f4dcc176a8"));
    vector<byte> wrapped = encoding::hex::decode("afbeb0f07dfbf5419200f2ccb50bb24f");
    println(encoding::hex::encode(wrapper.unwrap_padded(wrapped).value()));
}
```

Output:

```text
466f7250617369
```

## See also

- [wrap_padded](wrap_padded.md): the wrapped key
- [unwrap](unwrap.md): RFC 3394's
- [sgcl::crypto::aes_kw](README.md)
