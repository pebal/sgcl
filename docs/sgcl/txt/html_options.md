[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::html_options

```cpp
#include "sgcl/txt/html.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    struct html_options {
        bool scripting = true;
        bool collect_errors = false;
    };
}
```

`sgcl::txt::html_options` is how [html_document](html_document/README.md) parses.

## Member objects

| Field | Description |
|---|---|
| `scripting` | the standard's scripting flag: `true`, the default, reads a `<noscript>` as text, as a browser running scripts does; `false` reads its markup |
| `collect_errors` | keep the parse errors ([errors](html_document/errors.md)); `false` by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto on = txt::html_document::parse("<body><noscript><b>x</b></noscript>");
    auto off =
        txt::html_document::parse("<body><noscript><b>x</b></noscript>", {.scripting = false});
    println("{} | {}", int(on.body()[0][0].kind()), off.body()[0][0].name());
}
```

Output:

```text
3 | b
```

## See also

- [html_document::parse](html_document/parse.md)
- [sgcl::txt](README.md)
