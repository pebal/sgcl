[sgcl](../../README.md) › [crypto](../README.md) › [blake2b_512](README.md)

# sgcl::crypto::blake2b_512::reset

```cpp
void reset() noexcept;
```

Makes the hasher as the constructor made it: the message hashed so far is dropped, and the key, the salt and the
personalization it was made with are kept, so a keyed hasher goes on as a MAC under the same key for the next message.

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
    crypto::blake2b_256 mac({.key = "k"});
    mac.update("first message");
    mac.reset();
    mac.update("hello world");
    println(encoding::hex::encode(mac.value()));
}
```

Output:

```text
94d9f21e168de024709e8dea7e2424e2c049404344b94963ee24a473d9046d33
```

## See also

- [(constructor)](blake2b_512.md): what reset goes back to
- [sgcl::crypto::blake2b_512](README.md)
