[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::group, name, value

```cpp
string group() const noexcept;    // (1)
string name() const noexcept;     // (2)
string value() const noexcept;    // (3)
```

1. The group before the name, `item1` of `item1.EMAIL` (vCard's); empty without one.
2. The name, upper-cased.
3. The value as written, escapes kept.

## Parameters

None.

## Return value

The part, shared, not copied.

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
    auto line = encoding::content_line::parse("item1.note:a\\, b").value();
    println("[{}] [{}] [{}] [{}]", line.group(), line.name(), line.value(), line.text());
}
```

Output:

```text
[item1] [NOTE] [a\, b] [a, b]
```

## See also

- [param](param.md)
- [sgcl::encoding::content_line](README.md)
