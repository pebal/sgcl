[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::list, components

```cpp
vector<string> list() const noexcept;          // (1)
vector<string> components() const noexcept;    // (2)
```

A value of several texts, each read as [TEXT](text.md):

1. A list: split at the commas no backslash escapes (CATEGORIES, RESOURCES, vCard's NICKNAME).
2. A structured value: split at the semicolons no backslash escapes (vCard's N and ADR, REQUEST-STATUS).

## Parameters

None.

## Return value

The texts.

## Complexity

Linear in the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println(encoding::content_line("CATEGORIES", "WORK,MEETING\\, WEEKLY").list());
    auto adr = encoding::content_line("ADR", ";;123 Main Street;Springfield;IL;62701;USA").components();
    println("{} parts, street {}", adr.size(), adr[2]);
}
```

Output:

```text
["WORK", "MEETING, WEEKLY"]
7 parts, street 123 Main Street
```

## See also

- [text](text.md)
- [sgcl::encoding::content_line](README.md)
