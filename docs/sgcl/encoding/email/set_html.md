[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_html

```cpp
email& set_html(const string& html);
```

The HTML of the body, a text/html part in UTF-8 with its line breaks CRLF, put where a reader looks for it:
alone, or beside the text as multipart/alternative (the text first), inside multipart/mixed when there are
attachments. A HTML there was is replaced.

## Parameters

| Parameter | Description |
|---|---|
| `html` | the HTML |

## Return value

`*this`.

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
    encoding::email m("a@example.com", "b@example.com", "s", "first");
    println("{}", m.body().content_type());
    m.set_html("<b>rich</b>");
    println("{}", m.body().content_type());
    m.set_text("second");
    println("{} | {}", m.text(), m.html());
}
```

Output:

```text
text/plain
multipart/alternative
second | <b>rich</b>
```

## See also

- [text](text.md), [html](html.md)
- [email](README.md)
