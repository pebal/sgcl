[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md) › [token](xml-token.md)

# sgcl::encoding::xml::token::kind

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        class token {
        public:
            enum class kind : uint8_t {
                start_element, end_element, text, comment, instruction, doctype
            };
        };
    };
}
```

`sgcl::encoding::xml::token::kind` is what a [token](xml-token.md) is, as its [type](xml-token/type.md) tells it:
Go's `StartElement`, `EndElement`, `CharData`, `Comment`, `ProcInst` and `Directive`.

| Value | Description |
|---|---|
| `start_element` | the start of an element: its name, namespace and attributes; `<a/>` is a start and an end |
| `end_element` | the end of an element, with its name and namespace |
| `text` | a run of text: a CDATA section, or the text before or after one or a comment; the white space between elements |
| `comment` | a comment, its text in `text()` |
| `instruction` | a processing instruction, its target in `name()` and its data in `text()`; the XML declaration is one, named `xml` |
| `doctype` | the DOCTYPE declaration, whole and uninterpreted: the root it names in `name()`, all of it after the keyword in `text()` |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

const char* name_of(encoding::xml::token::kind k) {
    using enum encoding::xml::token::kind;
    switch (k) {
        case start_element: return "start_element";
        case end_element: return "end_element";
        case text: return "text";
        case comment: return "comment";
        case instruction: return "instruction";
        case doctype: return "doctype";
    }
    return "";
}

int main() {
    encoding::xml::reader r("<?xml version='1.0'?><!DOCTYPE a><a>t<!--c--></a>");
    while (auto t = r.next()) {
        println("{} [{}]", name_of(t->type()), t->name());
    }
}
```

Output:

```text
instruction [xml]
doctype [a]
start_element [a]
text []
comment []
end_element [a]
```

## See also

- [token::type](xml-token/type.md)
- [xml::kind](xml-kind.md): the kinds of the nodes of a tree
- [sgcl::encoding::xml::token](xml-token.md)
