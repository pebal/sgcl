[sgcl](../../README.md) › [txt](../README.md) › [stencil](README.md)

# sgcl::txt::stencil::steps

```cpp
size_t steps() const noexcept;
```

How many steps the source came to: a run of literal text and a field are one each, and a block a few more, for
where it branches, loops and jumps. Nothing a program needs; it is what a test asks to know that a page of text is
one step and not four hundred.

## Parameters

None.

## Return value

The number of steps.

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
    println("{}", txt::stencil("a page of literal text, however long").steps());
    println("{}", txt::stencil("Hello, {{ name }}!").steps());
    println("{}", txt::stencil("{{ if a }}yes{{ else }}no{{ end }}").steps());
    return 0;
}
```

Output:

```text
1
3
4
```

## See also

- [sgcl::txt::stencil](README.md)
