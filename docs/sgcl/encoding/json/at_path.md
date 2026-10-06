[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::at_path

```cpp
optional<json> at_path(const string& pointer) const noexcept;
```

The value at a JSON Pointer ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)), the `jsonpointer` of Go's x/exp:
`""` is this value, `"/users/0/name"` a member of an element of a member. In a key, `~1` stands for `/` and `~0`
for `~`. A token over an array is an index, digits with no leading zero; `-`, which names the place past the end,
names no value here.

Where [operator[]](operator_at.md) gives null for what is not there, `at_path` tells it apart: a member that is
null is a null, a member that is not there is `nullopt`.

## Parameters

| Parameter | Description |
|---|---|
| `pointer` | the JSON Pointer: empty, or `/` and the tokens separated by `/` |

## Return value

The value, or `nullopt` when there is none: a key that is not there, an index past the end or not an index, a
token over a number, a string, a boolean or null, or a text that is not a pointer (one that does not start with
`/`, a `~` followed by anything but `0` or `1`).

## Complexity

Linear in the length of the pointer, with a lookup at each token.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(
        R"({"users": [{"name": "Ala", "a/b": 1, "boss": null}]})");
    auto pointers = {"/users/0/name", "/users/0/a~1b", "/users/0/boss", "/users/0/age", "/users/01",
                     "/users/-", "users", ""};
    for (auto pointer : pointers) {
        auto v = doc.at_path(pointer);
        println("{:16} {}", pointer, v ? v->to_string() : string("nullopt"));
    }
}
```

Output:

```text
/users/0/name    "Ala"
/users/0/a~1b    1
/users/0/boss    null
/users/0/age     nullopt
/users/01        nullopt
/users/-         nullopt
users            nullopt
                 {"users":[{"name":"Ala","a/b":1,"boss":null}]}
```

## See also

- [set_path](set_path.md): the value with the one at a pointer replaced
- [operator[]](operator_at.md): a member or an element, one step
- [path_of](path_of.md): a pointer of keys
- [sgcl::encoding::json](README.md)
