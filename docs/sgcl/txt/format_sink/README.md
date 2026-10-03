[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::format_sink

```cpp
#include "sgcl/txt/format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class format_sink;
}
```

Where a value writes itself: room the caller lends, and a count. What does not fit is counted and not written, so one
pass both fills a buffer and says what the whole would take — the caller may size a buffer from an empty one and
write into it with the same code. It is what [format_to](../format_to.md) writes into and what a `format_value` or a
[formatter](../formatter/README.md) of the program is handed. Where `std::format` hands a formatter an output iterator, to
be written a character at a time, this is a pointer into a buffer, written in pieces.

## Rules

- A pointer, an end and a count: it lives anywhere, and it owns nothing. The room is the caller's and must outlive
  the writing.
- Nothing it does can fail or throw; a write past the end of the room is counted and dropped.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](format_sink.md) | puts a sink over room the caller lends |
| [put](put.md) | writes a character or a run of text |
| [fill](fill.md) | writes a character a number of times |
| [size](size.md) | what the whole text takes, whether or not it fitted |
| [reseat](reseat.md) | puts the sink over other room, keeping its count |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[8];
    txt::format_sink out(room, sizeof room);
    out.put("total");
    out.fill('.', 6);
    out.put('7');
    println("{} written of {}: {}", sizeof room, out.size(), std::string_view(room, sizeof room));
    return 0;
}
```

Output:

```text
8 written of 12: total...
```

## See also

- [format_to](../format_to.md): writes a pattern into one
- [write_padded](../write_padded.md): a text in its field, into one
- [growing_sink](../growing_sink/README.md): room that grows
