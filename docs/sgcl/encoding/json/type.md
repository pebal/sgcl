[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::type

```cpp
kind type() const noexcept;
```

The [kind](../json-kind.md) of the value: `null`, `boolean`, `number`, `string`, `array` or `object`, for a
`switch` where Go writes a type switch over an `any`. A number is `number` however it is held: an integer, a
double or a literal kept as its text.

## Parameters

None.

## Return value

The kind of the value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

string describe(const encoding::json& v) {
    switch (v.type()) {
        case encoding::json::kind::null: return "nothing";
        case encoding::json::kind::boolean: return v.as_bool(false) ? "yes" : "no";
        case encoding::json::kind::number: return "a number";
        case encoding::json::kind::string: return "the text " + v.as_string("");
        case encoding::json::kind::array: return "a list";
        case encoding::json::kind::object: return "a record";
    }
    return "?";
}

int main() {
    encoding::json doc = encoding::json::parse(
        R"([null, true, 1.5, 123456789012345678901, "hi", [], {}])");
    for (auto& v : doc.elements()) {
        println(describe(v));
    }
}
```

Output:

```text
nothing
yes
a number
a number
the text hi
a list
a record
```

## See also

- [kind](../json-kind.md): the kinds
- [is_null](is_null.md), [is_number](is_number.md), [is_object](is_object.md): one kind asked
- [sgcl::encoding::json](README.md)
