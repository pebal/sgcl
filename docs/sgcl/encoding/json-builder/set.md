[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [builder](../json-builder.md)

# sgcl::encoding::json::builder::set

```cpp
builder& set(const string& key, const json& value);
```

Adds a member of an object. On an empty builder it makes it a builder of an object, until [build](build.md)
empties it. A key set twice keeps its last value, in the place of its last `set`; [size](size.md) counts both
until `build`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the member |
| `value` | the value; anything a json is made of, `5`, `"text"`, another json |

## Return value

`*this`, for a chain of calls.

## Complexity

Amortized constant.

## Exceptions

`logic_error` when the builder is one of an array: a [push_back](push_back.md) came first. The builder is as it
was.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::builder user;
    user.set("name", "Ala").set("age", 30).set("admin", false);
    if (user.size() > 2) {
        user.set("age", 31);
    }
    println(user.size());
    println(user.build().to_string());
}
```

Output:

```text
4
{"name":"Ala","admin":false,"age":31}
```

## See also

- [push_back](push_back.md): an element of an array
- [build](build.md): the value
- [sgcl::encoding::json::builder](../json-builder.md)
