[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::text

```cpp
string text() const noexcept;
```

A scalar's text as it was written, the characters the core schema read: `0o14` of an integer, `1e3` of a float,
`~` of a null, a string's characters; what a constructor wrote for a scalar made here. Empty for a collection, and
for a null made here or left empty in the text.

## Parameters

None.

## Return value

The text.

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
    auto v = encoding::yaml::parse("[0o14, 1e3, ~, '', x]").value();
    for (const auto& e : v.elements()) {
        println("[{}] {}", e.text(), e.to_json().to_string());
    }
}
```

Output:

```text
[0o14] 12
[1e3] 1000
[~] null
[] ""
[x] "x"
```

## See also

- [as_string](as_string.md)
- [sgcl::encoding::yaml](README.md)
