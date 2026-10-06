[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::attribute

```cpp
optional<string> attribute(const string& name) const noexcept;
```

Returns the value of an element's attribute of a name (as the tokenizer lowercased it, or adjusted: `viewBox`).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the attribute's name |

## Return value

The value; `nullopt` without the attribute.

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
    auto doc = txt::html_document::parse(
        "<!DOCTYPE html><title>News</title><p id=lead>Hello "
        "<a href=\"/a\">first</a> and <a href=\"https://x.org/b\">second</a>. "
        "<template><b>later</b></template><svg viewbox=\"0 0 9 9\"><circle r=4></svg>");
    auto a = doc.body().elements("a");
    println("{} {}", *a[1].attribute("href"), a[1].attribute("title").has_value());
}
```

Output:

```text
https://x.org/b false
```

## See also

- [attributes](attributes.md)
- [sgcl::txt::html_node](README.md)
