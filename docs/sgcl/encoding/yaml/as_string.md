[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::as_string

```cpp
optional<string> as_string() const noexcept;                // (1)
string as_string(const string& fallback) const noexcept;    // (2)
```

The characters of a string, shared, not copied; `nullopt` for every other node — a number written plain is a number, [text](text.md) gives its characters.

1. The value, or `nullopt`.
2. The value, or `fallback` when there is none: `y["port"].as_string("?")`.

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
    auto v = encoding::yaml::parse("[plain, '12', 12]").value();
    println("{} {} {}", v[0].as_string(), v[1].as_string(), v[2].as_string());
    println(v[2].as_string("?"));
}
```

Output:

```text
"plain" "12" nullopt
?
```

## See also

- [type](type.md)
- [sgcl::encoding::yaml](README.md)
