[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::text

```cpp
writer& text(const string& t) noexcept;
```

Writes text, escaped: `&lt;`, `&amp;` and `&gt;`, a carriage return as `&#xD;`, which a reader would otherwise
make a line feed, and a character XML cannot hold — a control, invalid UTF-8 — as U+FFFD. The quotes stay as they
are, where Go's `EscapeText` escapes them. An empty text writes nothing. Inside an element, text ends the start tag,
and with an indentation no line is broken in the element from there on: its white space is content.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the text, as it is |

## Return value

`*this`.

## Complexity

Linear in the length of `t`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout, encoding::xml::pretty);
    w.start("p").text("a < b & \"c\"").start("b").text("bold").end().text("!").end();
    w.flush().value();
    println();
}
```

Output:

```text
<p>a &lt; b &amp; "c"<b>bold</b>!</p>
```

## See also

- [cdata](cdata.md): text in a CDATA section
- [sgcl::encoding::xml::writer](README.md)
