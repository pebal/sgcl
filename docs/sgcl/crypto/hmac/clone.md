[sgcl](../../README.md) › [crypto](../README.md) › [hmac](../hmac.md)

# sgcl::crypto::hmac\<H\>::clone

```cpp
hmac clone() const noexcept;
```

A second hmac under the same key, at the same point of its message: the copy a hasher makes by value, asked for by
name here, since the states it copies are the key's equivalent. A common prefix is hashed once and the two go on two
ways, or one key serves two messages without being given again. The clone zeroes its own states when it dies, as any
hmac does.

## Parameters

None.

## Return value

The new hmac.

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
    crypto::hmac_sha256 mac("Jefe");
    mac.update("what do ya want ");
    crypto::hmac_sha256 other = mac.clone();  // the prefix hashed once
    mac.update("for nothing?");
    other.update("for something?");
    println(encoding::hex::encode(mac.value()));
    println("{}", mac.value() == other.value());
}
```

Output:

```text
5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
false
```

## See also

- [(constructor)](hmac.md): an hmac under a key
- [reset](reset.md): the same key for a new message
- [sgcl::crypto::hmac\<H\>](../hmac.md)
