[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::is_array

```cpp
bool is_array() const noexcept;
```

Whether the value is an array, empty or not; [elements](elements.md) gives its elements.

## Parameters

None.

## Return value

`true` when the value is an array.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(R"([null, true, 2.0, 2.5, -1e19, "2", [], {}])");
    for (auto& v : doc.elements()) {
        println("{} {}", v.to_string(), v.is_array());
    }
    println("missing {}", doc[100].is_array());
}
```

Output:

```text
null false
true false
2 false
2.5 false
-10000000000000000000 false
"2" false
[] true
{} false
missing false
```

## See also

- [type](type.md): the kind of the value
- [elements](elements.md): the elements
- [sgcl::encoding::json](README.md)
