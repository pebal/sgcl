[sgcl](../../README.md) › [crypto](../README.md) › [blake3](README.md)

# sgcl::crypto::blake3::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& tag) const noexcept;
```

Checks whether `tag` is the start of the output over the input so far — with a key, the MAC: what a received tag is
checked with, never `==`. A tag may have any length from 1 to 64 bytes, since a shorter output is the start of a
longer one; the protocol fixes the length, 32 bytes as a rule. The bytes are compared in constant time
([constant_time](../constant_time/README.md)) and the output computed for the comparison is zeroed before the call
returns. An empty tag, or one past 64 bytes, is false. The hasher goes on.

## Parameters

| Parameter | Description |
|---|---|
| `tag` | the tag received |

## Return value

`true` when `tag` is the start of the output, `false` otherwise. `[[nodiscard]]`: a check whose result is dropped was
never made, and the compiler says so.

## Complexity

As [value](value.md), and the comparison in a time that depends only on the length.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key(32);
    for (int i : range(32)) {
        key[i] = byte(i);
    }
    vector<byte> tag = encoding::hex::decode("0978071f9c601ec34611813742454dd142c63ffb2cacac65a20b38253323bb00");
    crypto::blake3 mac(key);
    mac.update("message");
    println("{}", mac.verify(tag));
    tag[0] ^= byte(1);
    println("{}", mac.verify(tag));
}
```

Output:

```text
true
false
```

## See also

- [value](value.md): the tag itself
- [constant_time](../constant_time/README.md): the comparison it makes
- [sgcl::crypto::blake3](README.md)
