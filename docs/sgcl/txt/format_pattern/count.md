[sgcl](../../README.md) › [txt](../README.md) › [format_pattern](README.md)

# sgcl::txt::format_pattern\<A...\>::count

```cpp
constexpr size_t count() const noexcept;
```

Returns how many steps the pattern was read into, the ones [parts](parts.md) points at: a step is a run of literal
text and the field after it, and the text after the last field is a step of its own. A pattern keeps four; `0`
says it keeps none — it has more steps than that, or a number too large for a step — and is read where it runs,
which is correct and only a little slower.

## Parameters

None.

## Return value

The number of steps, `0` to `4`.

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
    println("{}", txt::format_pattern<int>("{} left").count());
    println("{}", txt::format_pattern<int>("{0} {0} {0} {0} {0}").count());
}
```

Output:

```text
2
0
```

## See also

- [parts](parts.md): the steps
- [sgcl::txt::format_pattern](README.md)
