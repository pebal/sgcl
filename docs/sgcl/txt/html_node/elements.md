[sgcl](../../README.md) › [txt](../README.md) › [html_node](README.md)

# sgcl::txt::html_node::elements

```cpp
vector<html_node> elements(const string& name = string()) const;
```

Returns the descendant elements in document order: all of them, or those of a local name.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the local name; empty for every element |

## Return value

The elements.

## Complexity

Linear in the size of the subtree.

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
    for (const auto &e : doc.root().elements("a")) {
        println("{}", e.text());
    }
    println("{}", doc.root().elements().size());
}
```

Output:

```text
first
second
10
```

## See also

- [element_by_id](element_by_id.md)
- [sgcl::txt::html_node](README.md)
