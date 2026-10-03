[sgcl](../../README.md) › [txt](../README.md) › [value](README.md)

# sgcl::txt::value::is_none

```cpp
bool is_none() const noexcept;
```

Whether the value holds nothing, `value_kind::none`: a value made with no arguments or of `nullptr`.

## Parameters

None.

## Return value

`true` when the value holds nothing.

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
    txt::value data = txt::object{{"note", nullptr}, {"zero", 0}};
    println("{} {}", data.find("note")->is_none(), data.find("zero")->is_none());
    return 0;
}
```

Output:

```text
true false
```

## See also

- [kind](kind.md)
- [truthy](truthy.md)
- [sgcl::txt::value](README.md)
