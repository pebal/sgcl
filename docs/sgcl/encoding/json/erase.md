[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::erase

```cpp
json erase(const string& key) const noexcept;
```

A new value: the object without the member under `key`, the others in their order. When there is no such member,
or the value is not an object, this value itself. This value stays as it was.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the member to leave out |

## Return value

The object without the member, or this value.

## Complexity

Linear in the number of members, which are copied as handles.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json user = encoding::json::parse(R"({"name": "Ala", "password": "x", "age": 30})");
    auto shown = user.erase("password");
    println(shown.to_string());
    println(user.to_string());
    println(shown.erase("password") == shown);
}
```

Output:

```text
{"name":"Ala","age":30}
{"name":"Ala","password":"x","age":30}
true
```

## See also

- [set](set.md): the object with a member set
- [sgcl::encoding::json](README.md)
