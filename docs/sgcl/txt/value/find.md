[sgcl](../../README.md) › [txt](../README.md) › [value](README.md)

# sgcl::txt::value::find

```cpp
const value* find(const string& name) const noexcept;
```

The value a name stands for inside this one, or null where this is not a mapping or holds no such name. A pointer and
not an `optional`: what comes back lives in the mapping and is not copied, and a template asks this once per field
of every row.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name |

## Return value

The value of the name, or null.

## Complexity

Constant on average: a mapping is an [ordered_map](../../core/ordered_map/README.md).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"user", txt::object{{"name", "Ada"}}}};
    const txt::value* name = data.find("user")->find("name");
    println("{} {}", *name->text(), data.find("nobody") == nullptr);
    return 0;
}
```

Output:

```text
Ada true
```

## See also

- [at](at.md): an element of a list
- [sgcl::txt::value](README.md)
