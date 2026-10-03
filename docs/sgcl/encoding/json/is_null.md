[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::is_null

```cpp
bool is_null() const noexcept;
```

Whether the value is null: made by the default [constructor](json.md) or from `nullptr`, read from `null`, or given
by a lookup that found nothing — [operator[]](operator_at.md) of a key that is not there, of an index past the end,
of a value that is not an object or an array.

## Parameters

None.

## Return value

`true` when the value is null.

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
        println("{} {}", v.to_string(), v.is_null());
    }
    println("missing {}", doc[100].is_null());
}
```

Output:

```text
null true
true false
2 false
2.5 false
-10000000000000000000 false
"2" false
[] false
{} false
missing true
```

## See also

- [type](type.md): the kind of the value
- [operator[]](operator_at.md): a member or an element, null when there is none
- [sgcl::encoding::json](../json.md)
