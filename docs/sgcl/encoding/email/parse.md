[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::parse

```cpp
static expected<email, error> parse(const string& text);                                // (1)
static expected<email, error> parse(const string& text, const limits& l);               // (2)
static expected<email, error> parse(const slice<const byte>& data);                     // (3)
static expected<email, error> parse(const slice<const byte>& data, const limits& l);    // (4)
template<class T>
static expected<email, error> parse(const T& text);                                     // (5)
template<class T>
static expected<email, error> parse(const T& text, const limits& l);                    // (6)
static expected<email, error> parse(const io::reader& in);                              // (7)
static expected<email, error> parse(const io::reader& in, const limits& l);             // (8)
```

A message read as the readers of mail read one: the head's fields (folded lines, the obsolete syntax, UTF-8),
the line that is not a field taken as the start of the body, an mbox's "From " line before the head skipped, the
MIME tree (multiparts by their boundaries, nested, a multipart/digest's parts as messages, a message/rfc822 part
read as a message of its own), each leaf's transfer encoding undone. LF alone is taken for CRLF.

- (1–2) A text; (3–4) bytes; (5–6) a literal, a character array, a `std::string_view`, read where it lies;
  (7–8) all of a reader.
- (1, 3, 5, 7) Within the default [limits](../email-limits.md); (2, 4, 6, 8) within `l`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the message |
| `data` | its bytes |
| `in` | where it is read from, to its end |
| `l` | the limits |

## Return value

The message, or the [error](../error/README.md): `errc::limit_exceeded` for a head past `max_header_bytes` or a part past `max_parts`, `errc::depth_limit` for nesting past `max_depth`, `errc::io` for a reader that failed. Nothing else is refused: a malformed part is read as well as it can be.

## Complexity

Linear in the size of the message.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string text = "From: John Doe <jdoe@machine.example>\nSubject: Hello\n\nThis is a message.\n";
    auto m = encoding::email::parse(text);
    println("{} | {} | {}", m->from()->name(), m->subject(), m->text());

    encoding::email::limits small;
    small.max_header_bytes = 16;
    auto refused = encoding::email::parse("Subject: a long subject line\r\n\r\n", small);
    println("{}", refused.error().message());
}
```

Output:

```text
John Doe | Hello | This is a message.

offset 0: header block too large
```

## See also

- [load, async_load](load.md)
- [limits](../email-limits.md)
- [email](README.md)
