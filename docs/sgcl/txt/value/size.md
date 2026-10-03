[sgcl](../../README.md) › [txt](../README.md) › [value](../value.md)

# sgcl::txt::value::size

```cpp
size_t size() const noexcept;
```

How many elements a list or a mapping holds; nought for everything else, which is what makes a `range` of a template
over a number take the empty road rather than a wrong one.

## Parameters

None.

## Return value

The number of elements.

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
    txt::value data = txt::object{{"xs", txt::list{1, 2, 3}}, {"name", "Ada"}};
    println("{} {} {}", data.size(), data.find("xs")->size(), data.find("name")->size());
    return 0;
}
```

Output:

```text
2 3 0
```

## See also

- [at](at.md), [find](find.md): an element
- [sgcl::txt::value](../value.md)
