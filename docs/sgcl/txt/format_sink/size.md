[sgcl](../../README.md) › [txt](../README.md) › [format_sink](README.md)

# sgcl::txt::format_sink::size

```cpp
constexpr size_t size() const noexcept;
```

What the whole text takes, whether or not it fitted: every character put or filled since the sink was made (or what
[reseat](reseat.md) was told had been counted, and everything since). When it is larger than the room, the room holds
the first characters and the rest were dropped.

## Parameters

None.

## Return value

The number of characters written and dropped together.

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
    txt::format_sink out(room, sizeof room);
    out.put("abcdef");
    println("{} counted, {} kept: {}", out.size(), sizeof room,
            std::string_view(room, sizeof room));
    return 0;
}
```

Output:

```text
6 counted, 4 kept: abcd
```

## See also

- [sgcl::txt::format_sink](README.md)
