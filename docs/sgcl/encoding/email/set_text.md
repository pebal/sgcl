[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_text

```cpp
email& set_text(const string& text);
```

The text of the body, a text/plain part in UTF-8 with its line breaks CRLF, put where a reader looks for it:
alone, or beside the html as multipart/alternative (the text first), inside multipart/mixed when there are
attachments. A text there was is replaced.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text |

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
