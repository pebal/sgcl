[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::operator=

```cpp
writer& operator=(writer&& other) noexcept = default;    // (1)
writer& operator=(const writer&) = delete;               // (2)
```

1. Takes the writing of `other` over, with its stream, what it gathered and not flushed, its open elements and its
   mistake; what this writer gathered and did not flush is dropped, and `other` is left to be destroyed or
   assigned to.
2. A writer is not copied: two writers would hold one pending text and write it twice.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the writer taken over |

## Return value

`*this`.

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
    encoding::xml::writer w(io::stdout);
    w.start("draft").text("dropped");
    encoding::xml::writer started(io::stdout);
    started.start("note").text("kept");
    w = std::move(started);
    w.end().flush().value();
    println();
}
```

Output:

```text
<note>kept</note>
```

## See also

- [(constructor)](xml-writer.md)
- [sgcl::encoding::xml::writer](README.md)
