[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::is_integer

```cpp
bool is_integer() const noexcept;
```

Whether the value is a number whose value is an integer that an `int64_t` or an `uint64_t` holds exactly, that is
whether [as_int](as_int.md) or [as_uint](as_uint.md) gives it: `2.0` and `1e2` are, `2.5`, `-1e19` and a literal
past 2^64 are not.

## Parameters

None.

## Return value

`true` when the value is a number that an `int64_t` or an `uint64_t` holds exactly.

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
        println("{} {}", v.to_string(), v.is_integer());
    }
    println("missing {}", doc[100].is_integer());
}
```

Output:

```text
null false
true false
2 true
2.5 false
-10000000000000000000 false
"2" false
[] false
{} false
missing false
```

## See also

- [type](type.md): the kind of the value
- [as_int](as_int.md), [as_uint](as_uint.md): the integer
- [sgcl::encoding::json](README.md)
