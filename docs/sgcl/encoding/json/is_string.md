[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::is_string

```cpp
bool is_string() const noexcept;
```

Whether the value is a string; [as_string](as_string.md) gives it. A number kept as its text is a number, not a
string.

## Parameters

None.

## Return value

`true` when the value is a string.

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
        println("{} {}", v.to_string(), v.is_string());
    }
    println("missing {}", doc[100].is_string());
}
```

Output:

```text
null false
true false
2 false
2.5 false
-10000000000000000000 false
"2" true
[] false
{} false
missing false
```

## See also

- [type](type.md): the kind of the value
- [as_string](as_string.md): the string
- [sgcl::encoding::json](README.md)
