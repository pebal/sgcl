[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::html

```cpp
string html() const;
```

The HTML of the body: its first text/html part that is not an attachment, as [text](text.md) takes the text.

## Parameters

None.

## Return value

The HTML; `""` when the body has none.

## Complexity

Linear in the size of the body.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "s", "plain");
    m.set_html("<p>rich</p>");
    println("{} | {}", m.text(), m.html());
}
```

Output:

```text
plain | <p>rich</p>
```

## See also

- [set_html](set_html.md)
- [email](README.md)
