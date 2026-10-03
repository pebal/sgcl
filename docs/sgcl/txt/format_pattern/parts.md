[sgcl](../../README.md) › [txt](../README.md) › [format_pattern](../format_pattern.md)

# sgcl::txt::format_pattern\<A...\>::parts

```cpp
constexpr const format_part* parts() const noexcept;
```

Returns the steps the pattern was read into where the program was compiled, [count](count.md) of them: each a run of
literal text and the field after it, its specification made out ([format_part](../format_part.md)). A call of
[format](../format.md) walks them in order. A pattern keeps four steps; one with more, or with a number too large for
a step, keeps none and is read where it runs.

## Parameters

None.

## Return value

A pointer to the first of the steps, held by the pattern; `count()` of them are read.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    constexpr txt::format_pattern<string, int> pattern("{} has {:>3} items");
    for (size_t i : range(pattern.count())) {
        const txt::format_part& p = pattern.parts()[i];
        println("[{}] then value {}", pattern.view().substr(p.at, p.size), int(p.which));
    }
}
```

Output:

```text
[] then value 0
[ has ] then value 1
[ items] then value 255
```

## See also

- [count](count.md): how many
- [format_part](../format_part.md): a step
- [sgcl::txt::format_pattern](../format_pattern.md)
