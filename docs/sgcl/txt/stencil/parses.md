[sgcl](../../README.md) › [txt](../README.md) › [stencil](../stencil.md)

# sgcl::txt::stencil::parses

```cpp
static bool parses(const string& source) noexcept;
```

Whether `source` is a template, read with the six functions every template has, and nothing kept: for a program that
reads a directory of them at startup and wants to say which one is broken before it needs any of them.

## Parameters

| Parameter | Description |
|---|---|
| `source` | the text of the template |

## Return value

`true` when [parse](parse.md) would give a template.

## Complexity

Linear in the length of `source`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    sorted_map<string, string> files = {{"letter.txt", "Dear {{ name }},"},
                                        {"report.txt", "{{ range rows }}{{ . }}"}};
    for (auto& [name, source] : files) {
        println("{}: {}", name, txt::stencil::parses(source) ? "ok" : "broken");
    }
    return 0;
}
```

Output:

```text
letter.txt: ok
report.txt: broken
```

## See also

- [parse](parse.md): the template, or where and why it is not one
- [sgcl::txt::stencil](../stencil.md)
