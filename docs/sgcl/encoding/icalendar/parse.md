[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::parse, async_parse

```cpp
static expected<icalendar, error> parse(const string& text) noexcept;                             // (1)
static expected<icalendar, error> parse(const string& text, const options& o) noexcept;           // (2)
static expected<icalendar, error> parse(const io::reader& in);                                    // (3)
static expected<icalendar, error> parse(const io::reader& in, const options& o);                  // (4)
static async::task<expected<icalendar, error>> async_parse(io::reader in) noexcept;               // (5)
static async::task<expected<icalendar, error>> async_parse(io::reader in, options o) noexcept;    // (6)
```

A text of exactly one VCALENDAR, by the [rules](README.md#rules) ([parse_all](parse_all.md) reads several).

1. Of the text, with the default [options](../icalendar-options.md).
2. The same with the options given.
3. Of the text of the stream, read to its end.
4. The same with the options given.
5. (3) for a task.
6. (4) for a task.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 (a byte order mark at its start passed over) |
| `in` | the stream |
| `o` | what is accepted |

## Return value

The calendar, or the [error](../error/README.md) with its line and column: `syntax` for a line that is no content
line, a line outside the VCALENDAR, a top component that is no VCALENDAR, none or two of them; `mismatched_tag` for
an END without its BEGIN or of another component; `unexpected_end` for a BEGIN without its END or a quote not closed;
`invalid_character`; `invalid_utf8`; `depth_limit`; `limit_exceeded` for a text past `max_size`; `io` for a stream
that failed.

## Complexity

Linear in the length of the text.

## Exceptions

- (1–2), (5–6) None.
- (3–4) What the read of the stream throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto calendar = encoding::icalendar::parse("BEGIN:VCALENDAR\nVERSION:2.0\nBEGIN:VEVENT\nSUMMARY:Hi\nEND:VEVENT\nEND:VCALENDAR\n");
    println(calendar->components()[0].text("SUMMARY", "?"));
    for (const char* bad : {"BEGIN:VCALENDAR\nEND:VEVENT\n", "BEGIN:VCALENDAR\nSUMMARY\nEND:VCALENDAR\n", "BEGIN:VEVENT\nEND:VEVENT\n"}) {
        println(encoding::icalendar::parse(bad).error().message());
    }
}
```

Output:

```text
Hi
2:1: END:VEVENT where END:VCALENDAR belongs
2:1: no ':' before the value
1:1: a top component that is no VCALENDAR: VEVENT
```

## See also

- [parse_all](parse_all.md)
- [load](load.md)
- [sgcl::encoding::icalendar](README.md)
