[sgcl](../../README.md) › [txt](../README.md) › [growing_sink](README.md)

# sgcl::txt::growing_sink::text

```cpp
string text() const;
```

The text, once the walk is over: the characters of [view](view.md), copied into a [string](../../core/string/README.md). Of
room that was [lent](../growing_sink-lent_t.md) and never added to there is no whole text to hand back — what did
not fit was dropped — so that road asks [size](size.md) instead.

## Parameters

None.

## Return value

The text written.

## Complexity

Linear in its length.

## Exceptions

`length_error` when the text passes 4 GiB, the most a string holds.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[4];
    txt::growing_sink page(room, sizeof room);
    size_t cap = page.capacity();
    for (auto word : {"to ", "be ", "or ", "not"}) {
        size_t mark = page.size();
        page.out().put(word);
        if (page.size() > cap) [[unlikely]] {
            cap = page.take_room(page.size(), mark);
            page.out().put(word);
        }
    }
    string whole = page.text();
    println("{:?}", whole);
    return 0;
}
```

Output:

```text
"to be or not"
```

## See also

- [view](view.md): the characters where they stand
- [sgcl::txt::growing_sink](README.md)
