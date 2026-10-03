[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::set

```cpp
json set(const string& key, const json& value) const noexcept;    // (1)
json set(size_t index, const json& value) const noexcept;         // (2)
```

A new value with a member or an element set; this value stays as it was.

1. The object with the member under `key` replaced, in its place, or added at the end when there is none. On a
   value that is not an object, an object of that one member.
2. The array with the element at `index` replaced. An index past the end, or a value that is not an array, gives
   this value unchanged; [push_back](push_back.md) appends.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the member |
| `index` | the position of the element, from 0 |
| `value` | the new value of the member or the element |

## Return value

The new value.

## Complexity

Linear in the number of members or elements, which are copied as handles.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json user = encoding::json::parse(R"({"name": "Ala", "age": 30})");
    auto older = user.set("age", 31).set("city", "Kraków");
    println(user.to_string());
    println(older.to_string());

    encoding::json list = encoding::json::array({1, 2, 3});
    println(list.set(1, "two").to_string());
    println(list.set(3, 4).to_string());
    println(encoding::json(5).set("k", true).to_string());
}
```

Output:

```text
{"name":"Ala","age":30}
{"name":"Ala","age":31,"city":"Kraków"}
[1,"two",3]
[1,2,3]
{"k":true}
```

## See also

- [erase](erase.md): the object without a member
- [set_path](set_path.md): a value deeper down replaced
- [builder](../json-builder.md): many members or elements without a copy per step
- [sgcl::encoding::json](../json.md)
