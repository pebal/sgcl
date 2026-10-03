[sgcl](../../README.md) › [crypto](../README.md) › [sha1](../sha1.md)

# sgcl::crypto::sha1::sha1

```cpp
sha1() noexcept;
```

Makes a hasher that has hashed nothing: the five chaining words at the initial values of FIPS 180-4, an empty block
and a length of zero. Its `value()` is the digest of the empty message. Copying a hasher is the other way to make
one: the copy goes on from where the original stood.

## Parameters

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
    println(encoding::hex::encode(h.value()));
}
```

Output:

```text
da39a3ee5e6b4b0d3255bfef95601890afd80709
```

## See also

- [update](update.md): hashes bytes in
- [reset](reset.md): back to what the constructor made
- [sgcl::crypto::sha1](../sha1.md)
