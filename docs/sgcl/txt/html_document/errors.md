[sgcl](../../README.md) › [txt](../README.md) › [html_document](README.md)

# sgcl::txt::html_document::errors

```cpp
slice<const html_parse_error> errors() const noexcept;
```

Returns the parse errors, in the order the parse met them, when [html_options](../html_options.md) asked for them;
none otherwise.

## Parameters

None.

## Return value

The errors: an [html_parse_error](../html_parse_error.md) each.

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

    txt::html_options o{.collect_errors = true};
    for (const auto& e : txt::html_document::parse("<p>a</div>&amp", o).errors()) {
        println("{} {}", e.offset, e.code);
    }
}
```

Output:

```text
2 missing-doctype
7 unexpected-end-tag
11 missing-semicolon-after-character-reference
```

## See also

- [html_parse_error](../html_parse_error.md)
- [html_options](../html_options.md)
- [sgcl::txt::html_document](README.md)
