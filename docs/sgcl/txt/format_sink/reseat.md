[sgcl](../../README.md) › [txt](../README.md) › [format_sink](README.md)

# sgcl::txt::format_sink::reseat

```cpp
constexpr void reseat(char* at, size_t room, size_t counted) noexcept;
```

Puts the same sink over other room, with what it has counted so far kept rather than started again: the next
character goes to `at`, and [size](size.md) goes on from `counted`. Nobody writing a value calls this: it is for
whoever owns the room and can get more of it, which is what [growing_sink](../growing_sink/README.md) does when a step runs
off the end — and why a reference to the sink stays good over the growth.

## Parameters

| Parameter | Description |
|---|---|
| `at` | where the next character goes |
| `room` | how many characters there are from `at` on |
| `counted` | the count to go on from, usually how many characters stand before `at` |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include <algorithm>

using namespace sgcl;

int main() {
    char small[4];
    char large[16];
    txt::format_sink out(small, sizeof small);
    out.put("ab");
    size_t mark = out.size();
    out.put("cdef");  // runs off the end
    std::copy_n(small, mark, large);
    out.reseat(large + mark, sizeof large - mark, mark);
    out.put("cdef");  // the step again, into room that holds it
    println("{}", std::string_view(large, out.size()));
    return 0;
}
```

Output:

```text
abcdef
```

## See also

- [growing_sink](../growing_sink/README.md): room that grows, built on this
- [sgcl::txt::format_sink](README.md)
