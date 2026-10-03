[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::to_string

```cpp
string to_string(const style& s = compact) const;
```

The node as XML text, in UTF-8: an element with everything inside it, a text escaped, a comment, an instruction;
an empty string for `xml()`. Go's `xml.Marshal` and `MarshalIndent` of a structure are the nearest.

- Text is written with `&lt;`, `&amp;` and `&gt;`, a carriage return as `&#xD;`, which a reader would otherwise
  make a line feed. An attribute's value, always in `"`, is written with `&quot;` too, and a tab, a line feed and
  a carriage return as character references, which a reader would otherwise make spaces.
- An element without content is written `<empty/>`.
- A character XML cannot hold at all — a control, invalid UTF-8 — is written as U+FFFD, as Go writes it.
- With an indentation in `s` ([pretty](../xml.md#member-objects)), each element stands on a line of its own,
  indented a level deeper than its parent; inside an element that holds text no line is broken from its text on,
  since there the white space is content. `s.declaration` writes `<?xml version="1.0" encoding="UTF-8"?>` first.

What `to_string` writes of a tree [parse](parse.md) read, `parse` reads back as the same tree. The tree is walked
with a loop of its own, not recursion. An indented text grows with the square of the depth; it is measured at
every line it begins, and the writing stops as soon as it passes what a string holds, so a tree deep enough for
terabytes of indentation fails at once.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the indentation and the XML declaration ([style](../xml-style.md)); `compact` by default |

## Return value

The text of the node.

## Complexity

Linear in the size of the subtree.

## Exceptions

`length_error` when the text would pass `string::max_size()`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml note = encoding::xml("note", "1 < 2 & \"so\"").set("title", "a \"b\"\tc");
    println(note.to_string());

    auto doc = encoding::xml::parse("<r><p>Hello <b>you</b>!</p><q><z/></q></r>").value();
    println(doc.to_string(encoding::xml::pretty));
    println(encoding::xml("e").to_string(encoding::xml::style{0, true}));
}
```

Output:

```text
<note title="a &quot;b&quot;&#x9;c">1 &lt; 2 &amp; "so"</note>
<r>
  <p>Hello <b>you</b>!</p>
  <q>
    <z/>
  </q>
</r>
<?xml version="1.0" encoding="UTF-8"?><e/>
```

## See also

- [save](save.md): the text into a file
- [stringify](stringify.md): the text of a value of a program's type
- [writer](../xml-writer.md): XML onto a stream, a call at a time
- [style](../xml-style.md)
- [sgcl::encoding::xml](../xml.md)
