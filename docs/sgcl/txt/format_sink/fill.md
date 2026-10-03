[sgcl](../../README.md) › [txt](../README.md) › [format_sink](README.md)

# sgcl::txt::format_sink::fill

```cpp
constexpr void fill(char c, size_t n) noexcept;
```

Writes the character `c` `n` times where the sink stands and moves it on; what does not fit in the room is counted
and dropped. It is the padding of a field, for a value of one's own that pads by itself rather than through
[write_padded](../write_padded.md).

## Parameters

| Parameter | Description |
|---|---|
| `c` | the character |
| `n` | how many times |

## Return value

None.

## Complexity

Linear in the number of characters that fit.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

namespace chart {
    struct bar { int length; };

    void format_value(txt::format_sink& out, bar b, const txt::format_spec&) {
        out.put('|');
        out.fill('#', size_t(b.length));
        out.put('|');
    }
}

int main() {
    println("{} {}", chart::bar{3}, chart::bar{7});
    return 0;
}
```

Output:

```text
|###| |#######|
```

## See also

- [put](put.md): a character or a run of text
- [sgcl::txt::format_sink](README.md)
