[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::operator== (sgcl::encoding::content_line)

```cpp
friend bool operator==(const content_line& a, const content_line& b) noexcept;
```

Whether the lines have the same group, name (in any case, as they are upper-cased), parameters in the same order
and value as written; `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the lines |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the lines.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = [](const char* t) { return encoding::content_line::parse(t).value(); };
    println("{} {}", p("summary:x") == p("SUMMARY:x"), p("SUMMARY:x") == p("SUMMARY;LANGUAGE=en:x"));
}
```

Output:

```text
true false
```

## See also

- [sgcl::encoding::content_line](README.md)
