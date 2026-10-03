[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::cdata

```cpp
writer& cdata(const string& t) noexcept;
```

Writes `t` as a CDATA section, `<![CDATA[t]]>`, nothing in it escaped. A text holding `]]>`, which would end the
section, is written as two sections, split inside it; a character XML cannot hold is written as U+FFFD. A reader
gives the section as text.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the text |

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
    encoding::xml::writer w(io::stdout);
    w.start("script").cdata("if (a[b[0]]>1 && c < d) {}").end();
    w.flush().value();
    println();
}
```

Output:

```text
<script><![CDATA[if (a[b[0]]]]><![CDATA[>1 && c < d) {}]]></script>
```

## See also

- [text](text.md): text, escaped
- [sgcl::encoding::xml::writer](../xml-writer.md)
