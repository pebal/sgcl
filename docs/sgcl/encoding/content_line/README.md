[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::content_line

```cpp
#include "sgcl/encoding/content_line.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class content_line;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::content_line` is one line of iCalendar ([RFC 5545](https://www.rfc-editor.org/rfc/rfc5545) §3.1) and
vCard ([RFC 6350](https://www.rfc-editor.org/rfc/rfc6350) §3.3): a name, its parameters and a value,
`DTSTART;TZID=Europe/Warsaw:20261006T090000`, and in a vCard a group before the name (`item1.EMAIL`). Immutable, one
word shared by copying: what [icalendar::property](../icalendar/README.md) and [vcard::property](../vcard/README.md)
name. The value is kept as a file writes it; [text](text.md) reads its escapes and the `as_*` functions its other
types, so one line of any type is held the same way.

## Rules

- **Names** — the property's, the parameters', the group — are letters, digits and `-`, compared in any case and kept
  upper-cased.
- **Parameter values** are a list separated by commas, each quoted when it holds `:`, `;` or `,`;
  [RFC 6868](https://www.rfc-editor.org/rfc/rfc6868)'s `^n`, `^^` and `^'` stand for a line break, a caret and a
  quote. vCard 3.0's bare value, `TEL;HOME:`, is a `TYPE`.
- **The value** may hold any character but the controls (tab allowed). A TEXT value escapes `\\`, `;`, `,` and the
  line break: [text](text.md) builds one, [text()](text.md) reads one.
- **Written**, a line is folded at 75 octets (a continuation starting with a space, never inside a UTF-8 sequence) and
  ended by CRLF.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `parameter` | a parameter's name and values: [content_line::parameter](../content_line-parameter.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](content_line.md) | a line of a name, parameters and a value as written |
| [text](text.md) | a line of a TEXT value (static); the value read as TEXT |
| [parse](parse.md) | one line of a file (static) |
| [to_string](to_string.md) | the line as a file holds it |

#### Observers

| Function | Description |
|---|---|
| [group, name, value](name.md) | the parts of the line |
| [params, param](param.md) | the parameters, one parameter's value |
| [list, components](list.md) | a value of several texts |
| [as_int, as_bool](as_int.md) | an INTEGER, a BOOLEAN |
| [as_date, as_datetime](as_date.md) | a DATE, a DATE-TIME |
| [as_duration, as_utc_offset](as_duration.md) | a DURATION, a UTC-OFFSET |
| [as_recurrence](as_recurrence.md) | an RRULE's rule |

#### New versions

| Function | Description |
|---|---|
| [with_param, with_group](with_param.md) | with a parameter set, with a group |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same group, name, parameters and value |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto line = encoding::content_line::parse("ATTENDEE;CN=\"Doe, Jane\";ROLE=CHAIR:mailto:jane@example.com").value();
    println("{} {} {}", line.name(), line.param("CN", "?"), line.value());
    encoding::content_line summary = encoding::content_line::text("SUMMARY", "Lunch, then a walk");
    print(summary.to_string());
    println(summary.text());
}
```

Output:

```text
ATTENDEE Doe, Jane mailto:jane@example.com
SUMMARY:Lunch\, then a walk
Lunch, then a walk
```

## See also

- [icalendar](../icalendar/README.md), [vcard](../vcard/README.md), [recurrence](../recurrence/README.md)
- [sgcl::encoding](../README.md)
