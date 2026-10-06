[sgcl](../../README.md) › [concurrent](../README.md) › [count_min_sketch](README.md)

# sgcl::concurrent::count_min_sketch::from_bytes

```cpp
static expected<count_min_sketch, error> from_bytes(const slice<const byte>& bytes) noexcept;
```

Reads a sketch from the bytes of [to_bytes](to_bytes.md): the header, the shape and the size of the input checked
before the counters are allocated.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the bytes of a sketch |

## Return value

The sketch, or an [error](../error/README.md) with the byte it stopped on: `too short for the sketch's header`, `not
the sketch's magic`, `an unknown version of the sketch's format`, `a reserved byte of a count-min sketch not zero`,
`a count-min sketch's width from 1 to 2^40`, `a count-min sketch's depth from 1 to 64`, `a count-min sketch's size does
not match its shape`.

## Complexity

Linear in the size of the input.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::count_min_sketch c;
    c.add("x", 9);
    auto back = concurrent::count_min_sketch::from_bytes(c.to_bytes());
    println("{} {}", back->estimate("x"), back->total());
}
```

Output:

```text
9 9
```

## See also

- [to_bytes](to_bytes.md)
- [sgcl::concurrent::count_min_sketch](README.md)
