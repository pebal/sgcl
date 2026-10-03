[sgcl](../../README.md) › [txt](../README.md) › [runtime_pattern](README.md)

# sgcl::txt::runtime_pattern::runtime_pattern

```cpp
explicit runtime_pattern(const string& text) noexcept;
```

Makes the pattern of `text`, keeping the string. Nothing is read: the pattern is read by the call that takes it.
Explicit, so that a text is never taken for a pattern read where the program runs without being asked for by name;
[runtime](../runtime.md) is the short way to say it.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the pattern |

## Complexity

Constant: the string is shared, not copied.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    string entry = "{0}/{1}";
    txt::runtime_pattern path(entry);
    println("{}", txt::format(path, "usr", "lib").value_or("?"));
    return 0;
}
```

Output:

```text
usr/lib
```

## See also

- [runtime](../runtime.md): the same as a function
- [sgcl::txt::runtime_pattern](README.md)
