[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::dotenv::parse, async_parse

```cpp
static expected<dotenv, error> parse(const string& text) noexcept;                             // (1)
static expected<dotenv, error> parse(const string& text, const options& o) noexcept;           // (2)
static expected<dotenv, error> parse(const io::reader& in);                                    // (3)
static expected<dotenv, error> parse(const io::reader& in, const options& o);                  // (4)
static async::task<expected<dotenv, error>> async_parse(io::reader in) noexcept;               // (5)
static async::task<expected<dotenv, error>> async_parse(io::reader in, options o) noexcept;    // (6)
```

The entries of a `.env` text, by the [rules](README.md#rules).

1. Of the text, with the default [options](../dotenv-options.md): expansion on, the environment read.
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
| `o` | what is done |

## Return value

The entries, or the [error](../error/README.md) with its line and column: `syntax` for a line that is no
`KEY=VALUE` (a key that is none, no `=`, more after a closing quote, a `${...}` without a name or of another
operator); `unexpected_end` for a quote or a `${` not closed; `missing_field` for `${NAME:?message}` of an unset
name, the message its text; `limit_exceeded` for values past `max_size`; `invalid_utf8`; `invalid_character` for a null character; `io` for a stream that
failed.

## Complexity

Linear in the length of the text, and in the keys for each expansion.

## Exceptions

- (1–2), (5–6) None.
- (3–4) What the read of the stream throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv::options o;
    o.use_environment = false;
    auto env = encoding::dotenv::parse("A=1\nB=\"${A}${A}\"\nC='${A}'\nD=${NOPE:-fallback}\n", o);
    for (const auto& [key, value] : env->members()) {
        println("{}={}", key, value);
    }
    for (const char* bad : {"A", "1A=x", "A='open", "A=${NOPE:?NOPE must be set}"}) {
        println(encoding::dotenv::parse(bad, o).error().message());
    }
}
```

Output:

```text
A=1
B=11
C=${A}
D=fallback
1:2: no '=' after the key A
1:1: a key expected (a letter or '_', then letters, digits, '_' and '.')
1:3: a value without its closing quote
1:3: NOPE: NOPE must be set
```

## See also

- [load](load.md)
- [dotenv::options](../dotenv-options.md)
- [sgcl::encoding::dotenv](README.md)
