[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::reset

```cpp
void reset() noexcept;
```

Makes the hasher as it was made: the input hashed so far is dropped, and the mode and the key are kept, so a keyed
hasher goes on as a MAC under the same key, and a derive_key hasher under the same context.

## Parameters

None.

## Return value

None.

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
    crypto::blake3 h;
    h.update("something else");
    h.reset();
    h.update("abc");
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85
```

## See also

- [(constructor)](blake3.md): what reset goes back to
- [sgcl::crypto::blake3](README.md)
