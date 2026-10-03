[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::contains

```cpp
bool contains(const string& key) const noexcept;
```

Whether the value is an object with a member under `key`. It tells a member that is null from one that is not
there, which [operator[]](operator_at.md) gives as null alike.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the member, compared character by character, case-sensitive |

## Return value

`true` when the value is an object and has the member; `false` otherwise, for a value that is not an object too.

## Complexity

Linear in the number of members up to 16, constant on average past them.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(R"({"boss": null, "name": "Ala"})");
    println("{} {}", doc.contains("boss"), doc["boss"].is_null());
    println("{} {}", doc.contains("age"), doc["age"].is_null());
    println(doc["name"].contains("length"));
}
```

Output:

```text
true true
false true
false
```

## See also

- [operator[]](operator_at.md): the value of a member
- [at_path](at_path.md): a value deeper down, `nullopt` when there is none
- [sgcl::encoding::json](../json.md)
