[sgcl](../../README.md) › [txt](../README.md) › [value](README.md)

# sgcl::txt::value::text

```cpp
const string* text() const noexcept;
```

The text the value holds, or null where it holds anything else: a number is not text here, and no conversion is
made ([to_string](to_string.md) makes one).

## Parameters

None.

## Return value

The text, or null.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value data = txt::object{{"name", "Ada"}, {"born", 1815}};
    println("{} {}", *data.find("name")->text(), data.find("born")->text() == nullptr);
    return 0;
}
```

Output:

```text
Ada true
```

## See also

- [to_string](to_string.md): any value as text
- [sgcl::txt::value](README.md)
