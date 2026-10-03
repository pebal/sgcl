[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::all_ascii

```cpp
static constexpr bool all_ascii(std::string_view s) noexcept;
```

Checks whether every byte of `s` is plain ASCII, below 128: [ascii_run](ascii_run.md) from the start reaching the
end. A text all ASCII is one whose case is a single bit a letter and whose bytes are its code points, which the
library's own text functions take a shorter path for.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the bytes of the text |

## Return value

`true` when every byte is below 128; `true` for an empty text.

## Complexity

Linear in `s.size()`, eight bytes at a time.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", utf8::all_ascii("hello"), utf8::all_ascii("żółw"), utf8::all_ascii(""));
}
```

Output:

```text
true false true
```

## See also

- [ascii_run](ascii_run.md): the length of a run of ASCII
- [sgcl::utf8](../utf8.md)
