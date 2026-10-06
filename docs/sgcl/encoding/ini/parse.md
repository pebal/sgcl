[sgcl](../../README.md) › [encoding](../README.md) › [ini](README.md)

# sgcl::encoding::ini::parse, async_parse

```cpp
static expected<ini, error> parse(const string& text) noexcept;                             // (1)
static expected<ini, error> parse(const string& text, const options& o) noexcept;           // (2)
static expected<ini, error> parse(const io::reader& in);                                    // (3)
static expected<ini, error> parse(const io::reader& in, const options& o);                  // (4)
static async::task<expected<ini, error>> async_parse(io::reader in) noexcept;               // (5)
static async::task<expected<ini, error>> async_parse(io::reader in, options o) noexcept;    // (6)
```

The sections of an INI text, by the [rules](README.md#rules).

1. Of the text, with the default [options](../ini-options.md).
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

The sections, or the [error](../error/README.md) with its line and column: `syntax` for a line that is no header,
no entry and no comment, an entry without its key, or more after a header's `]`; `duplicate_key` for a section or a
key given twice; `invalid_utf8`; `invalid_character` for a null character; `io` for a stream that failed.

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
    auto config = encoding::ini::parse("[a]\nx = 1\n  continued\n; comment\ny: two\n");
    println("[{}] [{}]", config->get("a", "x", "?"), config->get("a", "y", "?"));
    for (const char* bad : {"[a]\nlonely", "[a]\nx = 1\nx = 2", "[a]\n[a]", "[a] extra"}) {
        println(encoding::ini::parse(bad).error().message());
    }
}
```

Output:

```text
[1
continued] [two]
2:1: a line that is no section, no key = value and no comment
3:1: the key x given twice in [a]
2:1: the section [a] given twice
1:5: more on the line after a section's ']'
```

## See also

- [load](load.md)
- [ini::options](../ini-options.md)
- [sgcl::encoding::ini](README.md)
