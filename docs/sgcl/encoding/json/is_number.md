[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::is_number

```cpp
bool is_number() const noexcept;
```

Whether the value is a number, however it is held: an integer, a double or a literal kept as its text
([number_text](number_text.md)). [is_integer](is_integer.md) asks whether it is an integer.

## Parameters

None.

## Return value

`true` when the value is a number.

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
        println("{} {}", v.to_string(), v.is_number());
    }
    println("missing {}", doc[100].is_number());
}
```

Output:

```text
null false
true false
2 true
2.5 true
-10000000000000000000 true
"2" false
[] false
{} false
missing false
```

## See also

- [type](type.md): the kind of the value
- [is_integer](is_integer.md): a number that is an integer
- [as_double](as_double.md): the number
- [sgcl::encoding::json](README.md)
