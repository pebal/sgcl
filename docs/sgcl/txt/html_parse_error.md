[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::html_parse_error

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct html_parse_error {
        size_t offset;
        string code;
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::html_parse_error` is an error a parse recovered from, kept when [html_options](html_options.md) asks.

## Member objects

| Field | Description |
|---|---|
| `offset` | where, in bytes of the text after its line breaks were normalized |
| `code` | the standard's code: `unexpected-null-character`, `duplicate-attribute`, `missing-doctype`; the tree construction's own errors carry codes of this library (`unexpected-end-tag`) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::html_options o{.collect_errors = true};
    for (const auto& e : txt::html_document::parse("<!DOCTYPE html><a b=1 b=2>", o).errors()) {
        println("{} {}", e.offset, e.code);
    }
}
```

Output:

```text
24 duplicate-attribute
```

## See also

- [html_document::errors](html_document/errors.md)
- [sgcl::txt](README.md)
