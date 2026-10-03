[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::growing_sink

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class growing_sink {
    public:
        struct lent_t;
        static constexpr lent_t lent {};
    };
}
```

A [format_sink](format_sink.md) whose room can be added to, for a caller that writes a long text in steps and can
write one of them over again: the other half of what [format_to](format_to.md) does. It starts over room the caller
lends; when a step runs off the end, [take_room](growing_sink/take_room.md) takes twice as much, carries over what
stood before the step and hands back the new capacity, so that **one step** is written again — never the whole
text.

`format` does not use it: a text of a pattern that does not fit is written a second time instead, because a
pattern's second pass is `to_chars` and a copy over a handful of fields, which costs less than the copying a buffer
that doubles does. A page of a [stencil](stencil.md) is the other case, and is what this is for: there a second pass
is every branch, every row and every `upper` walked over again, and a page is kilobytes where a message is a line.

## Rules

- It holds no tracked pointer: the first room is the caller's, on its stack, so a text that fits there allocates
  nothing at all; what it takes afterwards is a plain array of characters, freed with the sink. It lives anywhere,
  and it is neither copied nor moved.
- What it asks of the caller is a mark — where the step just written began — and to be asked cheaply: read
  [capacity](growing_sink/capacity.md) once into a variable of the caller's, compare [size](growing_sink/size.md)
  with that after every step, mark the test `[[unlikely]]`, and take the answer of `take_room` back into the
  variable. Both halves of that were measured ([benchmarks](benchmarks.md#stencil)).
- [out](growing_sink/out.md) is the sink to write into, and a reference to it stays good over a growth.

## Member types

| Type | Definition |
|---|---|
| [lent_t](growing_sink-lent_t.md) | the tag of room that cannot be added to |

## Member objects

| Member | Description |
|---|---|
| `lent` | the tag, `lent_t`: `growing_sink(growing_sink::lent, room, n)` writes what fits and counts the rest |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](growing_sink/growing_sink.md) | puts the sink over the caller's room |
| [out](growing_sink/out.md) | the sink to write into |
| [capacity](growing_sink/capacity.md) | how much the room holds |
| [take_room](growing_sink/take_room.md) | more room, the step before the mark carried over |
| [size](growing_sink/size.md) | what the whole text takes |
| [view](growing_sink/view.md) | what was written, where it stands |
| [text](growing_sink/text.md) | what was written, as a string |

## Example

A long text written in steps into room that grows, one step written again when it ran off the end:

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[16];
    txt::growing_sink page(room, sizeof room);
    size_t cap = page.capacity();
    for (auto step : {"one ", "two ", "three ", "four ", "five"}) {
        size_t mark = page.size();
        page.out().put(step);
        if (page.size() > cap) [[unlikely]] {
            cap = page.take_room(page.size(), mark);
            page.out().put(step);  // there is room for it now
        }
    }
    println("{} ({} bytes of room)", page.text(), cap);
    return 0;
}
```

Output:

```text
one two three four five (32 bytes of room)
```

## See also

- [format_sink](format_sink.md): the sink it owns
- [format_to](format_to.md): a text in one piece into room the caller lends
- [render](stencil/render.md): what it is for
