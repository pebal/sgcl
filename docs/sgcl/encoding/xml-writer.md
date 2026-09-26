# sgcl::encoding::xml::writer

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class xml::writer;   // XML written onto a stream, a call at a time
}
```

A writer of XML — Go's `xml.Encoder` with `EncodeToken` — onto an `io::writer`: calls that make the document in order, each returning the writer so that they chain, gathered in memory until `flush()` writes them out. For a whole [tree](xml.md) there is `node(x)`, or `x.to_string()` without a stream.

## Rules

- **A mistake is kept, not thrown**: a name that is not a qualified name, an attribute after the content of an element began (or with no start tag), `end()` with no element open, the same attribute twice in a tag, a comment holding `--` or ending with `-`, an instruction named `xml` or holding `?>`, `declaration()` after something was written. The first one is kept — `last_error()` has it whole, with its code — nothing after it is written, and `flush()` gives it as an `io::error` of the encoding category. The failure of the stream under it is kept too: `flush()` gives it again.
- **Escaping**: text is written with `&lt;`, `&amp;` and `&gt;`; an attribute's value (always in `"`) with `&quot;` too, and a tab, a line feed and a carriage return as character references, which a reader would otherwise make spaces. A carriage return in text is `&#xD;`, which a reader would otherwise make a line feed. A character XML cannot hold at all — a control, invalid UTF-8 — is written as U+FFFD, as Go writes it. So what the writer writes, a reader reads back as what was given.
- **CDATA**: `cdata(t)` writes a CDATA section; one holding `]]>` is written as two, split inside it.
- An element with no content is written `<empty/>`.
- `value(name, v)` writes a value of a program's type (`describe(field_list&)`, mapped as [`xml`](xml.md) says) as the element `name`; a value with no form in XML (a map, a variant...) is a mistake kept as the others are.
- **Indentation** (`style::indent`): each element on a line of its own, indented a level deeper than its parent — except inside an element that holds text, whose content is written as it is. `style::declaration` writes `<?xml version="1.0" encoding="UTF-8"?>` first.
- **Namespaces** are the program's: the writer makes no declarations of its own, and `xmlns` and `xmlns:p` are attributes like any other.
- The writer does not check that the document has one root: it writes fragments too (the stanzas of a stream that never ends, as XMPP writes).
- `flush()` writes on the thread that calls it; `async_flush()` in a task gives the worker back while the stream waits. What was gathered is written in one write, so call `flush()` between the parts of a large document.
- A writer holds tracked pointers: it lives where a `tracked_ptr` may.

## Members

```cpp
class xml::writer {
public:
    explicit writer(const io::writer& out, const style& s = compact);

    writer& declaration();                                  // <?xml version="1.0" encoding="UTF-8"?>
    writer& start(const string& name);
    writer& attribute(const string& name, const string& value);
    writer& text(const string& t);
    writer& cdata(const string& t);
    writer& comment(const string& t);
    writer& instruction(const string& target, const string& data = {});
    writer& end();                                          // the element started last
    writer& node(const xml& n);                             // a tree, whole
    template<class T>
    writer& value(const string& name, const T& v);          // a program's type as the element `name`

    expected<void, io::error> flush();
    async::task<expected<void, io::error>> async_flush();

    const optional<error>& last_error() const noexcept;
    size_t depth() const noexcept;                          // the elements open
};
```

## Example

```cpp
#include "sgcl/core/range.h"
#include "sgcl/encoding/xml.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout, encoding::xml::style{2, true});
    w.start("svg").attribute("xmlns", "http://www.w3.org/2000/svg").attribute("width", "40");
    for (auto i : range(2)) {
        w.start("circle").attribute("r", string(std::to_string(5 + 5 * i))).end();
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
    io::stdout.write("\n" + bad.last_error()->message() + "\n");
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
offset 4: an attribute with no start tag open to hold it
```

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `xml.NewEncoder(w)`, `EncodeToken` | `encoding::xml::writer(out)`, `start`, `attribute`, `text`, `end`… | a mistake kept and given by `flush()` |
| `Encoder.Indent(prefix, indent)` | `style{indent}` | no prefix; text content left as it is |
| `Encoder.Flush`, `Close` | `flush()`, `async_flush()` | the stream stays open |
| `xml.EscapeText` | what `text` and `attribute` do | Go escapes `"` and `'` in text too |
| `Encoder.Encode(v)`, `EncodeElement(v, start)` of a structure | `value("name", v)`; `node(x)` of a tree | |

## See also

[`xml`](xml.md), the tree; [`xml::reader`](xml-reader.md); [`error`](error.md).
