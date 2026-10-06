[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::with_param, with_group

```cpp
content_line with_param(const string& name, const string& value) const noexcept;    // (1)
content_line with_group(const string& group) const noexcept;                        // (2)
```

New lines; the line itself never changes.

1. With the parameter of the name set to one value: in its place when it is there, at the end when it is not.
2. With the group set (empty: none).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the parameter's name |
| `value` | its value |
| `group` | the group |

## Return value

The new line.

## Complexity

Linear in the parameters.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::content_line email("EMAIL", "jan@example.com");
    print(email.with_param("TYPE", "work").with_group("item1").to_string());
}
```

Output:

```text
item1.EMAIL;TYPE=work:jan@example.com
```

## See also

- [param](param.md)
- [sgcl::encoding::content_line](README.md)
