[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](../growing_sink.md)

# sgcl::txt::growing_sink::take_room

```cpp
size_t take_room(size_t want, size_t mark) noexcept;
```

More room: at least `want` characters of it and at least twice the capacity, with what stands before `mark` carried
over and the sink put back there, its count `mark`. The caller then writes again the one step that began at `mark`.
Of room that was [lent](../growing_sink-lent_t.md) nothing changes.

The caller writes its step first and asks afterwards, with `want` the size the step came to and `mark` where it
began. It is not that a step could not be measured in advance — a literal run of a page knows its own length — but
that asking in advance buys nothing and costs the same, which was measured both ways. What does cost is the branch,
and only if it is written the wrong way round: the caller marks the test `[[unlikely]]`, so a text that fits falls
through it; left to itself the compiler put the growth in line and jumped over it for the common case.

## Parameters

| Parameter | Description |
|---|---|
| `want` | how many characters the room must hold, the [size](size.md) the step came to |
| `mark` | where the step began; what stands before it is kept |

## Return value

The new [capacity](capacity.md), for the caller's variable; `size_t(-1)` for lent room.

## Complexity

Linear in `mark`, the characters carried over.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[8];
    txt::growing_sink page(room, sizeof room);
    page.out().put("kept ");
    size_t mark = page.size();
    page.out().put("then this step");
    size_t cap = page.take_room(page.size(), mark);
    println("{} characters, {:?} kept", cap, page.view());
    page.out().put("then this step");
    println("{:?}", page.view());
    return 0;
}
```

Output:

```text
19 characters, "kept " kept
"kept then this step"
```

## See also

- [capacity](capacity.md): read once, before the walk
- [sgcl::txt::growing_sink](../growing_sink.md)
