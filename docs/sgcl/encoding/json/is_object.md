[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::is_object

```cpp
bool is_object() const noexcept;
```

Whether the value is an object, empty or not; [members](members.md) gives its members.

## Parameters

None.

## Return value

`true` when the value is an object.

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
        println("{} {}", v.to_string(), v.is_object());
    }
    println("missing {}", doc[100].is_object());
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
[] false
{} true
missing false
```

## See also

- [type](type.md): the kind of the value
- [members](members.md): the members
- [sgcl::encoding::json](README.md)
