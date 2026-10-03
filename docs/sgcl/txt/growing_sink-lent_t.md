[sgcl](../README.md) › [txt](README.md) › [growing_sink](growing_sink.md) › lent_t

# sgcl::txt::growing_sink::lent_t

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class growing_sink {
    public:
        struct lent_t {};
        static constexpr lent_t lent {};
    };
}
```

The tag of room the caller lends that cannot be added to, written by its constant `growing_sink::lent`:
`growing_sink(growing_sink::lent, room, n)`. What fits is written and the whole size still comes back, which is the
contract of [format_to](format_to.md) and of [render_to](stencil/render_to.md), so that a walk that writes into
either kind of room has one kind of sink and not two. The [capacity](growing_sink/capacity.md) of such a sink is
`size_t(-1)` — nothing ever runs off the end of a room that big — and [take_room](growing_sink/take_room.md) changes
nothing; [view](growing_sink/view.md) and [text](growing_sink/text.md) give only the characters the room holds.

## Rules

- An empty type, passed by value.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[8];
    txt::growing_sink page(txt::growing_sink::lent, room, sizeof room);
    page.out().put("a text longer than the room");
    println("{} of {}: {}", page.view().size(), page.size(), page.view());
    println("{}", page.capacity() == size_t(-1));
    return 0;
}
```

Output:

```text
8 of 27: a text l
true
```

## See also

- [growing_sink](growing_sink/growing_sink.md): the constructor that takes it
- [sgcl::txt::growing_sink](growing_sink.md)
