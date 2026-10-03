[sgcl](../../README.md) › [crypto](../README.md) › [sha256](README.md)

# sgcl::crypto::sha256::reset

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
    crypto::sha256 h;
    for (const char* message : {"abc", "", "abc"}) {
        h.reset();
        h.update(message);
        println(encoding::hex::encode(h.value()));
    }
}
```

Output:

```text
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
```

## See also

- [(constructor)](sha256.md): a hasher of nothing yet
- [sgcl::crypto::sha256](README.md)
