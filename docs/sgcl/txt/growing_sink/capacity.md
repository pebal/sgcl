[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](../growing_sink.md)

# sgcl::txt::growing_sink::capacity

```cpp
size_t capacity() const noexcept;
```

How much the room holds: what may be written before more is asked for. `size_t(-1)` for room that was
[lent](../growing_sink-lent_t.md) and cannot grow.

A caller that writes in a loop reads this **once**, keeps it in a variable of its own and compares against that,
rather than asking here at every step, and takes the answer of [take_room](take_room.md) back into it. The reason is
register allocation: a field of this object has to be read back after every call a step makes, because a call might
have written it, where a local the compiler can prove nobody else reaches stays in a register across the whole walk.

## Parameters

None.

## Return value

The capacity in characters.

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
    char room[10];
    txt::growing_sink page(room, sizeof room);
    print("{}", page.capacity());
    page.out().put("twelve chars");
    print(" {}", page.take_room(page.size(), 0));
    page.out().put("twelve chars and more");
    println(" {}", page.take_room(page.size(), 0));
    return 0;
}
```

Output:

```text
10 20 40
```

## See also

- [take_room](take_room.md): more room, and the new capacity
- [sgcl::txt::growing_sink](../growing_sink.md)
