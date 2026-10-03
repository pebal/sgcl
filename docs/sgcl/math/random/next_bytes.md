[sgcl](../../README.md) › [math](../README.md) › [random](README.md)

# sgcl::math::random::next_bytes

```cpp
void next_bytes(const slice<byte>& out) noexcept;
```

Fills `out` from the stream, eight bytes from each draw, little-endian. The bytes of a last draw that are not
wanted are dropped, so every call starts on a new draw: two calls of 4 bytes take two draws, where one call of 8
takes one.

The bytes are for a simulation, a test, a replay; keys, nonces and tokens come from
[crypto::random](../../crypto/random/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `out` | the bytes filled: an array, a [vector](../../core/vector/README.md) of `byte` or a part of one |

## Return value

None.

## Complexity

Linear in the size of `out`: one draw for every eight bytes and one for the rest.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    math::random r(42);
    vector<byte> bytes(12);
    r.next_bytes(bytes);
    for (byte b : bytes) {
        print("{:02x}", int(b));
    }
    println("");

    math::random again(42);
    println("{:#x}", again.next_uint64());
}
```

Output:

```text
22301fb8d82978daf007b056
0xda7829d8b81f3022
```

## See also

- [next_uint64](next_uint64.md): the words the bytes are made of
- [sgcl::math::random](README.md)
