[sgcl](../../README.md) › [crypto](../README.md) › [sha1](../sha1.md)

# sgcl::crypto::sha1::reset

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
    crypto::sha1 h;
    h.update("something else");
    h.reset();
    h.update("abc");
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
a9993e364706816aba3e25717850c26c9cd0d89d
```

## See also

- [(constructor)](sha1.md): a hasher of nothing yet
- [sgcl::crypto::sha1](../sha1.md)
