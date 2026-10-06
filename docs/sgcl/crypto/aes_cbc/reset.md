[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::reset

```cpp
void reset(const slice<const byte>& iv);
```

Sets the chain to `iv`: the start of the next message under the same key, its IV fresh and unpredictable, without
setting the key up again.

## Parameters

| Parameter | Description |
|---|---|
| `iv` | the next message's IV, 16 bytes |

## Return value

None.

## Complexity

Constant.

## Exceptions

`invalid_argument` when `iv` is not 16 bytes; `logic_error` when the object was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key(16), iv(16);
    crypto::aes_cbc cbc(key, iv);
    vector<byte> first = cbc.encrypt("the same message");
    cbc.reset(iv);
    vector<byte> again = cbc.encrypt("the same message");
    println("{}", first == again);
}
```

Output:

```text
true
```

## See also

- [encrypt](encrypt.md): a message
- [sgcl::crypto::aes_cbc](README.md)
