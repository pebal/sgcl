[sgcl](../../README.md) › [crypto](../README.md) › [nonce_counter](README.md)

# sgcl::crypto::nonce_counter::operator=

```cpp
nonce_counter& operator=(nonce_counter&& other) noexcept;    // (1)
nonce_counter& operator=(const nonce_counter&) = delete;     // (2)
```

1. Takes the place of `other` over, in place of this counter's own; `other` is spent, and its `next()` throws
   `out_of_range`. An assignment of a counter to itself does nothing.
2. A counter is not copied: the copy would hand out the same nonces twice.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the counter taken over |

## Return value

`*this`.

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
    crypto::nonce_counter nonces;
    nonces.next();
    crypto::nonce_counter kept;
    kept = std::move(nonces);
    println("{}", encoding::hex::encode(kept.next()));
    try {
        nonces.next();
    } catch (const out_of_range& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
000000000000000000000001
sgcl::crypto::nonce_counter: every nonce has been used (or the counter was moved from)
```

## See also

- [(constructor)](nonce_counter.md): a counter, or another's taken over
- [sgcl::crypto::nonce_counter](README.md)
