[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::is_bool

```cpp
bool is_bool() const noexcept;
```

Whether the value is a boolean, `true` or `false`; [as_bool](as_bool.md) gives it.

## Parameters

None.

## Return value

`true` when the value is a boolean.

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
        println("{} {}", v.to_string(), v.is_bool());
    }
    println("missing {}", doc[100].is_bool());
}
```

Output:

```text
null false
true true
2 false
2.5 false
-10000000000000000000 false
"2" false
[] false
{} false
missing false
```

## See also

- [type](type.md): the kind of the value
- [as_bool](as_bool.md): the boolean
- [sgcl::encoding::json](../json.md)
