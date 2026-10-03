[sgcl](../../README.md) › [txt](../README.md) › [format_sink](../format_sink.md)

# sgcl::txt::format_sink::put

```cpp
/*(1)*/ constexpr void put(char c) noexcept;
/*(2)*/ constexpr void put(const char* text, size_t n) noexcept;
/*(3)*/ constexpr void put(std::string_view text) noexcept;
```

Writes characters where the sink stands and moves it on; what does not fit in the room is counted and dropped.

1. The character `c`.
2. The `n` characters at `text`, copied whole rather than a character at a time.
3. The characters of `text`: a string, a slice of characters or a literal converts.

Nothing is padded: a value of one's own that wants the fill, the alignment and the width of its field writes through
[write_padded](../write_padded.md).

## Parameters

| Parameter | Description |
|---|---|
| `c` | the character |
| `text` | the characters |
| `n` | how many characters `text` points at |

## Return value

None.

## Complexity

- (1) Constant.
- (2–3) Linear in the number of characters that fit.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    char room[16];
    txt::format_sink out(room, sizeof room);
    string name = "Ada";
    out.put('<');
    out.put(name.view());
    out.put("/>", 2);
    println("{}", std::string_view(room, out.size()));
    return 0;
}
```

Output:

```text
<Ada/>
```

## See also

- [fill](fill.md): one character a number of times
- [write_padded](../write_padded.md): a text in its field
- [sgcl::txt::format_sink](../format_sink.md)
