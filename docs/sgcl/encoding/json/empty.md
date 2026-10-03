[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::empty

```cpp
bool empty() const noexcept;
```

Whether the value has no elements or members: `size() == 0`. An empty array or object is empty, and so is every
value that is neither, an empty string or not.

## Parameters

None.

## Return value

`true` when [size](size.md) is 0.

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
    encoding::json doc = encoding::json::parse(R"([[], {}, [0], "text", null])");
    for (auto& v : doc.elements()) {
        println("{} {}", v.to_string(), v.empty());
    }
}
```

Output:

```text
[] true
{} true
[0] false
"text" true
null true
```

## See also

- [size](size.md): the number of elements or members
- [is_null](is_null.md): whether the value is null
- [sgcl::encoding::json](README.md)
