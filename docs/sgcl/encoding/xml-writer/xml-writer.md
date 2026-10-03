[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::writer

```cpp
/*(1)*/ explicit writer(const io::writer& out, const style& s = compact) noexcept;
/*(2)*/ writer(writer&& other) noexcept = default;
/*(3)*/ writer(const writer&) = delete;
```

Constructs a writer.

1. A writer onto the stream `out`, writing in the style `s`: on one line by default, or indented by `s.indent`
   spaces a level; with `s.declaration`, the XML declaration is the first thing gathered. Nothing is written onto
   the stream before [flush](flush.md).
2. Takes the writing of `other` over, with what it gathered and not flushed, its open elements and its mistake;
   `other` is left to be destroyed or assigned to.
3. A writer is not copied, as a [reader](../xml-reader/xml-reader.md) is not: two writers would hold one pending
   text and write it twice.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the stream the text is written onto |
| `s` | the indentation and the XML declaration ([style](../xml-style.md)); `compact` by default |
| `other` | the writer taken over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout, encoding::xml::pretty);
    w.start("list").start("item").text("one").end().start("item").end().end();
    w.flush().value();
    println();
}
```

Output:

```text
<list>
  <item>one</item>
  <item/>
</list>
```

## See also

- [flush](flush.md): what was gathered onto the stream
- [operator=](operator_assign.md): another writer taken over
- [style](../xml-style.md)
- [sgcl::encoding::xml::writer](../xml-writer.md)
