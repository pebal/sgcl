[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::vcard

```cpp
#include "sgcl/encoding/vcard.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class vcard;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::vcard` is one vCard — a contact of [RFC 6350](https://www.rfc-editor.org/rfc/rfc6350)'s version
4.0, and of RFC 2426's 3.0 as address books still hold them — its properties
([content_line](../content_line/README.md)s) in order, groups and parameters kept. Immutable, one word shared by
copying. [parse](parse.md) reads one card, [parse_all](parse_all.md) an address book, [to_string](to_string.md)
writes one.

## Rules

- **What is read**: lines unfolded as iCalendar's are, groups (`item1.EMAIL`), parameters with their lists and
  quotes, 3.0's and 2.1's bare TYPE values (`TEL;HOME;VOICE:` is two TYPEs), names in any case. vCard 2.1's
  QUOTED-PRINTABLE values are not decoded.
- **What is refused**: a BEGIN inside a card, an END without its BEGIN, a BEGIN without its END, a component that is
  no VCARD, a line that is no content line, control characters, invalid UTF-8, a text past `max_size`. Errors have
  their line and column.
- **Written**: VERSION right after BEGIN (RFC 6350 asks for it there), the other properties in order, each line folded
  at 75 octets and ended by CRLF.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `property` | [content_line](../content_line/README.md) |
| `options` | what a parse accepts: [vcard::options](../vcard-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](vcard.md) | a card of VERSION:4.0 |
| [parse, async_parse](parse.md) | a card of a text or a stream (static) |
| [parse_all](parse_all.md) | every card of a text (static) |
| [to_string](to_string.md) | the card as a file holds it |
| [load, load_all, async_load](load.md) | the cards of a file (static) |
| [save, async_save](save.md) | the card into a file |

#### Observers

| Function | Description |
|---|---|
| [version, formatted_name](version.md) | VERSION, FN |
| [properties, property_of, properties_of](properties.md) | the properties, those of a name |
| [text](text.md) | a property's text |

#### New versions

| Function | Description |
|---|---|
| [add](add.md) | with a property added |
| [set](set.md) | with a property replacing those of its name |
| [erase](erase.md) | without the properties of a name |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same properties in the same order |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto card = encoding::vcard::parse(
        "BEGIN:VCARD\r\nVERSION:4.0\r\nFN:Jan Kowalski\r\nN:Kowalski;Jan;;;\r\n"
        "EMAIL;TYPE=work:jan@example.com\r\nEMAIL;TYPE=home:jan@home.example\r\n"
        "TEL;VALUE=uri;TYPE=cell:tel:+48-600-000-000\r\nORG:Example\\, Inc.\r\nEND:VCARD\r\n").value();
    println("{} works at {}", card.formatted_name(), card.text("ORG", "?"));
    for (const auto& email : card.properties_of("EMAIL")) {
        println("{}: {}", email.param("TYPE", "other"), email.value());
    }
}
```

Output:

```text
Jan Kowalski works at Example, Inc.
work: jan@example.com
home: jan@home.example
```

## See also

- [content_line](../content_line/README.md), [icalendar](../icalendar/README.md)
- [sgcl::encoding](../README.md)
