[sgcl](../../README.md) › [encoding](../README.md) › [toml](README.md)

# sgcl::encoding::toml::parse, async_parse

```cpp
static expected<toml, error> parse(const string& text) noexcept;                             // (1)
static expected<toml, error> parse(const string& text, const options& o) noexcept;           // (2)
static expected<toml, error> parse(const io::reader& in);                                    // (3)
static expected<toml, error> parse(const io::reader& in, const options& o);                  // (4)
static async::task<expected<toml, error>> async_parse(io::reader in) noexcept;               // (5)
static async::task<expected<toml, error>> async_parse(io::reader in, options o) noexcept;    // (6)
```

A document: its root table, an empty one for a text of nothing but comments.

1. Of the text, with the default [options](../toml-options.md).
2. The same with the options given.
3. Of the text of the stream, read to its end.
4. The same with the options given.
5. (3) for a task.
6. (4) for a task.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `in` | the stream |
| `o` | what is accepted |

## Return value

The table, or the [error](../error/README.md) with its line and column: `syntax` for what the grammar does not
take (a key without its `=`, a value that is none of TOML's, two pairs on a line, a header over a value, a newline
in an inline table); `unexpected_end` for a string, an array or an inline table not closed; `invalid_escape`;
`invalid_character` for a control character; `invalid_utf8`; `duplicate_key` for a key or a table defined twice, or
extended where TOML forbids it; `out_of_range` for an integer past 64 bits; `depth_limit`; `io` for a stream that
failed.

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
    auto doc = encoding::toml::parse("name = 'app'\nports = [80, 443]\n");
    println("{} {}", doc->operator[]("name").as_string("?"), doc->operator[]("ports").size());
    for (const char* bad : {"a = 1\na = 2", "a = 012", "[t]\n[t]", "a = {x = 1}\na.y = 2", "a = [1, 2"}) {
        println(encoding::toml::parse(bad).error().message());
    }
}
```

Output:

```text
app 2
2:1: the key a defined twice
1:5: a value that is no string, number, boolean, date or time: 012
2:1: the table t defined twice
2:1: a dotted key through a, defined already
1:10: no ',' or ']' after an element of an array
```

## See also

- [to_string](to_string.md)
- [toml::options](../toml-options.md)
- [sgcl::encoding::toml](README.md)
