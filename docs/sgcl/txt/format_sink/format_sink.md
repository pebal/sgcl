[sgcl](../../README.md) › [txt](../README.md) › [format_sink](../format_sink.md)

# sgcl::txt::format_sink::format_sink

```cpp
constexpr format_sink(char* at, size_t room) noexcept;
```

Puts a sink over `room` characters at `at`, with nothing counted yet. Room of no characters is allowed: everything is
then counted and nothing written, which is how a caller asks what a text takes.

## Parameters

| Parameter | Description |
|---|---|
| `at` | the first character of the room; may be null when `room` is 0 |
| `room` | how many characters there are |

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
    txt::format_sink count(nullptr, 0);
    txt::write_padded(count, "żółć", txt::format_spec{.align = '^', .width = 10});
    println("{} bytes", count.size());
    return 0;
}
```

Output:

```text
14 bytes
```

## See also

- [size](size.md): what was counted
- [sgcl::txt::format_sink](../format_sink.md)
