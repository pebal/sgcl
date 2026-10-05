[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md)

# sgcl::encoding::email::part

```cpp
#include "sgcl/encoding/email.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class email {
    public:
        class part;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::email::part` is a part of a message's MIME tree (RFC 2045, RFC 2046): its head, and its content
(a leaf), its [parts](parts.md) (a multipart) or the [message](message.md) it holds (message/rfc822). What
[email::body](../email/body.md) and [email::attachments](../email/attachments.md) give, and what a program builds a
body of its own shape from ([email::set_body](../email/set_body.md)). Go reads the tree with `mime/multipart`'s
`Reader` and `Part`; Python's `email.message.Message` is a part as well.

A part's [content](content.md) is kept with its transfer encoding undone; [text](text.md) converts it from its
charset to UTF-8. Its head is read and set by name as the message's is; the parameters of Content-Type
([param](param.md), [charset](charset.md)) and the file's name of Content-Disposition ([filename](filename.md),
RFC 2231 continuations and charsets decoded) have calls of their own.

## Rules

- A handle, as [email](../email/README.md) is: one word; a copy is the same part, and a part taken from a message
  is that message's: what is set through it is the message's ([operator==](operator_cmp.md) says whether two are one).
- A part built by the program gets its type's Content-Type (`charset=utf-8` added to a text type without one), its
  content with line breaks CRLF for a text; the writer chooses the transfer encoding.
- The head of the root part is the message's head.
- Not thread-safe: one thread or task changes a part at a time.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](email-part.md) | a part of text or bytes, or an empty multipart |

#### Content

| Function | Description |
|---|---|
| [content](content.md) | the content, its transfer encoding undone |
| [text](text.md) | the content as text in UTF-8 |
| [parts](parts.md) | the parts of a multipart |
| [add](add.md) | a part after the others of a multipart |
| [message](message.md) | the message a message/rfc822 part holds |
| [is_multipart](is_multipart.md) | whether it is a multipart |
| [is_attachment](is_attachment.md) | whether a reader offers to save it |

#### Type and disposition

| Function | Description |
|---|---|
| [content_type](content_type.md) | the media type, `text/plain` |
| [param](param.md) | a parameter of Content-Type |
| [charset](charset.md) | the charset |
| [filename](filename.md) | the name of the file |
| [disposition](disposition.md) | `attachment`, `inline` or none |
| [content_id](content_id.md) | the Content-ID |
| [set_filename](set_filename.md) | Content-Disposition with a file's name |
| [set_content_id](set_content_id.md) | a Content-ID, for an HTML that shows the part |

#### The head

| Function | Description |
|---|---|
| [header](header.md) | the first value of a field |
| [header_all](header_all.md) | every value of a field |
| [has_header](has_header.md) | whether there is a field of the name |
| [headers](headers.md) | every field in its order |
| [set_header](set_header.md) | a field's value |
| [add_header](add_header.md) | a field at the end |
| [remove_header](remove_header.md) | every field of a name taken out |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two handles are one part |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


void show(const encoding::email::part& p, int depth) {
    println("{}{} [{}]", string(size_t(depth * 2), ' '), p.content_type(), p.filename());
    for (auto& c : p.parts()) {
        show(c, depth + 1);
    }
}

int main() {
    encoding::email m("a@example.com", "b@example.com", "Report", "See the file.");
    m.set_html("<p>See the file.</p>");
    m.attach("data.csv", vector<byte>(3, byte('x')));
    show(m.body(), 0);
}
```

Output:

```text
multipart/mixed []
  multipart/alternative []
    text/plain []
    text/html []
  text/csv [data.csv]
```

## See also

- [email](../email/README.md)
- RFC 2045, RFC 2046, RFC 2183, RFC 2231
