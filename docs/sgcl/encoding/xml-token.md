[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md)

# sgcl::encoding::xml::token

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        class token;
    };
}
```

`sgcl::encoding::xml::token` is one token of a document, as [reader](xml-reader.md)'s
[next](xml-reader/next.md) and [peek](xml-reader/peek.md) give it: the start of an element with its name, its
namespace and its attributes, its end, a run of text, a comment, a processing instruction, or the DOCTYPE
declaration, whole and uninterpreted ([token::kind](xml-token-kind.md)).

A token is a value: its names, attributes and text are strings of its own, kept as long as the token is — after
the reader has moved on, in a container, in another thread — where Go's `CharData` is valid until the next call to
`Token`. The short strings (names, the white space between elements, short values) are shared across the
document: a thousand `<book>` elements hold one `"book"`.

## Rules

- `<a/>` is a start and an end.
- A CDATA section is text, and the text of an element may come in more than one token: before and after a CDATA
  section or a comment (Go splits it the same way). The white space between elements is text; outside the root,
  only white space is allowed.
- The XML declaration is an instruction named `xml`, its text `version="1.0" encoding="UTF-8"`, as Go gives it.
- The DOCTYPE declaration's name is the root's name it gives, its text all of it after its keyword
  (`html PUBLIC ...`).
- A token is made by the reader; a token constructed by the program is an empty text.
- A token holds strings, which are tracked words: it lives where a `tracked_ptr` may. It is copied, as a value.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `StartElement`, `EndElement`, `CharData`, `Comment`, `ProcInst`, `Directive` | `token::kind::start_element`, `end_element`, `text`, `comment`, `instruction`, `doctype`; a token keeps its strings |
| `StartElement.Name`, `Attr` | [name](xml-token/name.md), [local_name](xml-token/local_name.md), [namespace_uri](xml-token/namespace_uri.md); [attributes](xml-token/attributes.md) |

## Member types

| Type | Definition |
|---|---|
| [kind](xml-token-kind.md) | what a token is |

## Member functions

#### Observers

| Function | Description |
|---|---|
| [type](xml-token/type.md) | what the token is |
| [name](xml-token/name.md) | the name of an element, the target of an instruction, the root a DOCTYPE names |
| [local_name](xml-token/local_name.md) | the name of an element without its prefix |
| [namespace_uri](xml-token/namespace_uri.md) | the namespace of an element's name |
| [text](xml-token/text.md) | the text, the comment, the data of an instruction, the DOCTYPE declaration |

#### Lookup

| Function | Description |
|---|---|
| [attributes](xml-token/attributes.md) | the attributes of a start, in order |
| [attribute](xml-token/attribute.md) | the value of an attribute of a start |
| [is_start](xml-token/is_start.md) | checks whether the token is the start of an element of a name |
| [is_end](xml-token/is_end.md) | checks whether the token is the end of an element of a name |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<a id='1'>x<![CDATA[<y>]]><b/></a>");
    vector<encoding::xml::token> kept;
    while (auto t = r.next()) {
        kept.push_back(*t);
    }
    for (auto& t : kept) {
        println("{} [{}] [{}]", int(t.type()), t.name(), t.text());
    }
}
```

Output:

```text
0 [a] []
2 [] [x]
2 [] [<y>]
0 [b] []
1 [b] []
1 [a] []
```

## See also

- [reader](xml-reader.md): what gives the tokens
- [token::kind](xml-token-kind.md)
- [sgcl::encoding::xml](xml.md)
