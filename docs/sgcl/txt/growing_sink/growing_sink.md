[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](../growing_sink.md)

# sgcl::txt::growing_sink::growing_sink

```cpp
/*(1)*/ growing_sink(char* room, size_t n) noexcept;
/*(2)*/ growing_sink(lent_t, char* room, size_t n) noexcept;
/*(3)*/ growing_sink(const growing_sink&) = delete;
```

1. Puts the sink over `n` characters of the caller's at `room`, which more can be added to: the capacity is `n`,
   and [take_room](take_room.md) grows it into memory of the sink's own.
2. Puts the sink over room of the caller's which cannot be added to ([lent_t](../growing_sink-lent_t.md)): what fits
   is written and the whole size still counted; the capacity reads `size_t(-1)`, so a walk never asks for more.
3. Not copied, and not moved: the sink inside it is reached by reference for the whole walk.

## Parameters

| Parameter | Description |
|---|---|
| `room` | the caller's room, which must outlive the sink |
| `n` | how many characters there are |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[4];
    txt::growing_sink grows(room, sizeof room);
    txt::growing_sink lent(txt::growing_sink::lent, room, sizeof room);
    println("{} {}", grows.capacity(), lent.capacity() == size_t(-1));
    return 0;
}
```

Output:

```text
4 true
```

## See also

- [take_room](take_room.md): more room
- [sgcl::txt::growing_sink](../growing_sink.md)
