[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::instruction

```cpp
writer& instruction(const string& target, const string& data = {}) noexcept;
```

Writes a processing instruction, `<?target data?>` (`<?target?>` without data), a character XML cannot hold as
U+FFFD. A target that is not a name without a colon, or is `xml` in any case — the declaration is
[declaration](declaration.md)'s — and data holding `?>` or starting with white space are mistakes (`errc::syntax`), kept and given by
[flush](flush.md).

## Parameters

| Parameter | Description |
|---|---|
| `target` | the target of the instruction |
| `data` | what follows the target |

## Return value

`*this`.

## Complexity

Linear in the length of `target` and `data`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout);
    w.declaration().instruction("xml-stylesheet", R"(type="text/xsl" href="a.xsl")");
    w.start("doc").end();
    w.flush().value();
    println();

    encoding::xml::writer bad(io::stdout);
    bad.instruction("xml");
    println(bad.last_error()->message());
}
```

Output:

```text
<?xml version="1.0" encoding="UTF-8"?><?xml-stylesheet type="text/xsl" href="a.xsl"?><doc/>
'xml' cannot be the target of an instruction
```

## See also

- [declaration](declaration.md): the XML declaration
- [comment](comment.md)
- [sgcl::encoding::xml::writer](../xml-writer.md)
