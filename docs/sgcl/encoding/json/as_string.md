[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::as_string

```cpp
optional<string> as_string() const noexcept;                // (1)
string as_string(const string& fallback) const noexcept;    // (2)
```

The string of the value, its characters with the escapes of the text decoded. The [string](../../core/string/README.md)
given is the value's own, shared, not copied. Nothing is converted: a number, kept as its text or not, is not a
string ([number_text](number_text.md) gives its literal, [to_string](to_string.md) its text).

1. The string, or `nullopt` when the value is not one.
2. The string, or `fallback`: `doc["user"]["name"].as_string("?")`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when the value is not a string |

## Return value

The string; (1) `nullopt`, (2) `fallback` when the value is not one.

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
    encoding::json doc = encoding::json::parse(R"({"user": {"name": "Ala ❤", "id": 7}})");
    println(doc["user"]["name"].as_string("?"));
    println(doc["user"]["id"].as_string("?"));
    println(doc["user"]["name"].as_string());
    println(doc["nobody"]["name"].as_string());
}
```

Output:

```text
Ala ❤
?
"Ala ❤"
nullopt
```

## See also

- [is_string](is_string.md): whether the value is a string
- [to_string](to_string.md): the text of any value
- [sgcl::encoding::json](README.md)
