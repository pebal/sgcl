[sgcl](../../README.md) › [txt](../README.md) › [object](../object.md)

# sgcl::txt::object::object

```cpp
/*(1)*/ object() noexcept;
/*(2)*/ object(std::initializer_list<pair<string, value>> fields) noexcept;
```

1. An empty mapping, to be filled with [set](set.md).
2. A mapping of `fields`, in the order they are written; a name written twice keeps its first place and its last
   value. A value of a field may be a list in braces of its own, `{"tags", {"one", "two"}}`, or another mapping,
   `{"user", txt::object{{"name", "Ada"}}}`.

## Parameters

| Parameter | Description |
|---|---|
| `fields` | the names and their values |

## Complexity

- (1) Constant.
- (2) Linear in the number of fields, on average.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value empty = txt::object();
    txt::value point = txt::object{{"y", 2}, {"x", 1}, {"y", 3}};
    println("{} {} {}", empty, point, empty.kind() == txt::value_kind::object);
    return 0;
}
```

Output:

```text
{} {"y": 3, "x": 1} true
```

## See also

- [set](set.md): a field at a time
- [sgcl::txt::object](../object.md)
