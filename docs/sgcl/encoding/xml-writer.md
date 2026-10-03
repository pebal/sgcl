[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md)

# sgcl::encoding::xml::writer

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        class writer;
    };
}
```

`sgcl::encoding::xml::writer` writes XML onto an [io::writer](../io/writer.md) — Go's `xml.Encoder`
with `EncodeToken`: calls that make the document in order, each returning the writer so that they chain, gathered
in memory until [flush](xml-writer/flush.md) writes them out. For a whole [tree](xml.md) there is
[node](xml-writer/node.md), or the tree's own [to_string](xml/to_string.md) without a stream; for a value of a
program's type, [value](xml-writer/value.md).

## Rules

- **A mistake is kept, not thrown**: a name that is not a qualified name, an attribute after the content of an
  element began (or with no start tag), `end()` with no element open, the same attribute twice in a tag, a
  comment holding `--` or ending with `-`, an instruction named `xml` or holding `?>`, `declaration()` after
  something was written, a value with no form in XML. The first one is kept — [last_error](xml-writer/last_error.md)
  has it whole, its code and its words, with no place, since it came from no input text — nothing after it is
  written, and `flush()` gives it as an `io::error` of the `encoding` category, writing nothing of what was gathered
  since the last flush. The failure of the stream under it is kept too: `flush()` gives it again.
- **Escaping**: text is written with `&lt;`, `&amp;` and `&gt;`; an attribute's value (always in `"`) with
  `&quot;` too, and a tab, a line feed and a carriage return as character references, which a reader would
  otherwise make spaces. A carriage return in text is `&#xD;`, which a reader would otherwise make a line feed. A
  character XML cannot hold at all — a control, invalid UTF-8 — is written as U+FFFD, as Go writes it. So what the
  writer writes, a reader reads back as what was given.
- An element with no content is written `<empty/>`.
- **Indentation** ([style](xml-style.md)`::indent`): each element on a line of its own, indented a level deeper
  than its parent — but inside an element that holds text, from the text on, whose content is written as it is.
  `style::declaration` writes `<?xml version="1.0" encoding="UTF-8"?>` first.
- **Namespaces** are the program's: the writer makes no declarations of its own, and `xmlns` and `xmlns:p` are
  attributes like any other.
- The writer does not check that the document has one root: it writes fragments too (the stanzas of a stream that
  never ends, as XMPP writes).
- `flush()` writes on the thread that calls it; `async_flush()` in a task gives the worker back while the stream
  waits. What was gathered is written in one write, so `flush()` is called between the parts of a large document.
  A writer destroyed without a flush writes nothing.
- A writer holds tracked pointers: it lives where a `tracked_ptr` may. It is moved, not copied, as the
  [reader](xml-reader.md) is: two writers would hold one pending text and write it twice.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `xml.NewEncoder(w)`, `EncodeToken` | `xml::writer(out)`, `start`, `attribute`, `text`, `end`…: a mistake kept and given by `flush()` |
| `Encoder.Indent(prefix, indent)` | `style{indent}`: no prefix; text content left as it is |
| `Encoder.Flush`, `Close` | `flush()`, `async_flush()`: the stream stays open |
| `xml.EscapeText` | what `text` and `attribute` do; Go escapes `"` and `'` in text too |
| `Encoder.Encode(v)`, `EncodeElement(v, start)` of a structure | `value("name", v)`; `node(x)` of a tree |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xml-writer/xml-writer.md) | a writer onto a stream, in a style, or one taken over |
| `(destructor)` | drops the writer and what was not flushed |
| [operator=](xml-writer/operator_assign.md) | takes another writer over |

#### Writing

| Function | Description |
|---|---|
| [declaration](xml-writer/declaration.md) | the XML declaration, first |
| [start](xml-writer/start.md) | the start tag of an element |
| [attribute](xml-writer/attribute.md) | an attribute of the element just started |
| [text](xml-writer/text.md) | text, escaped |
| [cdata](xml-writer/cdata.md) | a CDATA section |
| [comment](xml-writer/comment.md) | a comment |
| [instruction](xml-writer/instruction.md) | a processing instruction |
| [end](xml-writer/end.md) | the end of the element started last |
| [node](xml-writer/node.md) | a node of a tree, whole |
| [value](xml-writer/value.md) | a value of a program's type as an element |
| [flush, async_flush](xml-writer/flush.md) | writes what was gathered onto the stream |

#### Observers

| Function | Description |
|---|---|
| [last_error](xml-writer/last_error.md) | the first mistake |
| [depth](xml-writer/depth.md) | the elements open |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout, encoding::xml::style{2, true});
    w.start("svg").attribute("xmlns", "http://www.w3.org/2000/svg").attribute("width", "40");
    for (int i : range(2)) {
        w.start("circle").attribute("r", to_string(5 + 5 * i)).end();
    }
    w.start("text").text("5 < 10 & \"quoted\"").end();
    w.comment(" made by hand ");
    w.node(encoding::xml("desc", "a tree, whole"));
    w.end();
    if (auto r = w.flush(); !r) {
        return 1;
    }

    encoding::xml::writer bad(io::stdout);
    bad.start("a").text("t").attribute("late", "1");
    println("\n{}", bad.last_error()->message());
}
```

Output:

```text
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="40">
  <circle r="5"/>
  <circle r="10"/>
  <text>5 &lt; 10 &amp; "quoted"</text>
  <!-- made by hand -->
  <desc>a tree, whole</desc>
</svg>
an attribute with no start tag open to hold it
```

## See also

- [reader](xml-reader.md): XML a token at a time
- [xml](xml.md): the tree, and its `to_string`
- [style](xml-style.md); [error](error.md)
- [sgcl::encoding::xml](xml.md)
