[sgcl](../../README.md) › [encoding](../README.md) › [field](../field.md)

# sgcl::encoding::field::required

```cpp
field& required() noexcept;
```

Marks the field required: reading an object without it is `missing_field`, with the field's path. By default a
field that is not there keeps the value it had. JSON checks the object's keys, XML the element's children and
attributes, CSV the header's columns. Writing is not changed. Go has no such option; v2 neither.

## Parameters

None.

## Return value

`*this`, for the next option in the chain.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct login {
    string user;
    int attempts = 3;

    void describe(encoding::field_list& f) {
        f.add("user", user).required();
        f.add("attempts", attempts);
    }
};

int main() {
    auto ok = encoding::json::parse<login>(R"({"user": "ala"})");
    println("{} {}", ok->user, ok->attempts);
    auto missing = encoding::json::parse<login>(R"({"attempts": 1})");
    println(missing.error().message());

    encoding::csv::reader rows(string("attempts\n5\n"));
    rows.read<login>();
    println(rows.last_error()->message());
}
```

Output:

```text
ala 3
1:15 /user: missing field
2:1 /user: missing column
```

## See also

- [omit_empty](omit_empty.md): the other side, an empty field not written
- [sgcl::encoding::field](../field.md)
