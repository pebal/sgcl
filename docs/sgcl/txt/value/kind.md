[sgcl](../../README.md) › [txt](../README.md) › [value](../value.md)

# sgcl::txt::value::kind

```cpp
value_kind kind() const noexcept;
```

What the value holds: one of the seven shapes of [value_kind](../value_kind.md).

## Parameters

None.

## Return value

The kind of the value.

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
    txt::value data = txt::object{{"n", 7}, {"xs", txt::list{1, 2}}};
    println("{} {}", data.find("n")->kind() == txt::value_kind::integer,
            data.find("xs")->kind() == txt::value_kind::list);
    return 0;
}
```

Output:

```text
true true
```

## See also

- [value_kind](../value_kind.md)
- [is_none](is_none.md)
- [sgcl::txt::value](../value.md)
