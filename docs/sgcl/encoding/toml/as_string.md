[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::as_string

```cpp
optional<string> as_string() const noexcept;                // (1)
string as_string(const string& fallback) const noexcept;    // (2)
```

The characters of a string, its escapes read, shared, not copied; `nullopt` for every other value — [text](text.md) gives the characters of a number or a date.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `t["port"].as_string("?")`.

## Parameters

| Parameter | Description |
|---|---|
| `fallback` | what (2) gives when there is no value |

## Return value

(1) The value, or `nullopt`; (2) the value, or `fallback`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::toml::parse("a = \"tab\\there\"\nb = 'C:\\path'\nc = 12").value();
    println("{} {} {}", v["a"].as_string(), v["b"].as_string(), v["c"].as_string());
    println(v["c"].as_string("?"));
}
```

Output:

```text
"tab\there" "C:\\path" nullopt
?
```

## See also

- [type](type.md)
- [sgcl::encoding::toml](README.md)
