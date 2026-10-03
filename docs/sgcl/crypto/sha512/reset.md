[sgcl](../../README.md) › [crypto](../README.md) › [sha512](README.md)

# sgcl::crypto::sha512::reset

```cpp
void reset() noexcept;
```

Puts the hasher back to what the constructor made: the initial values, an empty buffer, a length of zero. One hasher
then serves message after message. The bytes of the message dropped are overwritten as the next ones come in, not
zeroed at once.

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
    crypto::sha512_256 h;
    for (const char* message : {"a", "abc"}) {
        h.reset();
        h.update(message);
        println(encoding::hex::encode(h.value()));
    }
}
```

Output:

```text
455e518824bc0601f9fb858ff5c37d417d67c2f8e0df2babe4808858aea830f8
53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23
```

## See also

- [(constructor)](sha512.md): a hasher of nothing yet
- [sgcl::crypto::sha512](README.md)
