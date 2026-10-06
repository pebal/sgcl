[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::parse, async_parse

```cpp
static expected<vcard, error> parse(const string& text) noexcept;                             // (1)
static expected<vcard, error> parse(const string& text, const options& o) noexcept;           // (2)
static expected<vcard, error> parse(const io::reader& in);                                    // (3)
static expected<vcard, error> parse(const io::reader& in, const options& o);                  // (4)
static async::task<expected<vcard, error>> async_parse(io::reader in) noexcept;               // (5)
static async::task<expected<vcard, error>> async_parse(io::reader in, options o) noexcept;    // (6)
```

A text of exactly one card, by the [rules](README.md#rules) ([parse_all](parse_all.md) reads an address book).

1. Of the text, with the default [options](../vcard-options.md).
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

The card, or the [error](../error/README.md) with its line and column: `syntax` for a line that is no content line,
a component that is no VCARD, none or two cards; `mismatched_tag`; `unexpected_end`; `depth_limit` for a BEGIN inside
a card; `invalid_character`; `invalid_utf8`; `limit_exceeded`; `io` for a stream that failed.

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
    auto card = encoding::vcard::parse("BEGIN:VCARD\nVERSION:3.0\nFN:Ann\nTEL;HOME;VOICE:555-0100\nEND:VCARD\n");
    auto tel = card->property_of("TEL");
    println("{} {} {}", card->version(), tel->params().size(), tel->param("TYPE", "?"));
    println(encoding::vcard::parse("BEGIN:VCARD\nFN:Ann\n").error().message());
}
```

Output:

```text
3.0 2 HOME
3:1: BEGIN:VCARD without its END
```

## See also

- [parse_all](parse_all.md)
- [sgcl::encoding::vcard](README.md)
