[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::dotenv

```cpp
#include "sgcl/encoding/dotenv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class dotenv;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::dotenv` is the entries of a `.env` file — the variables a program reads at its start — as docker
compose, python-dotenv and Go's godotenv read them: keys and their values in the file's order, immutable, one word
shared by copying. [parse](parse.md) and [load](load.md) read them, [to_string](to_string.md) and [save](save.md) write
them, [get](get.md) and its typed forms look them up, and [apply](apply.md) puts them into the process environment.
There is no standard of `.env`; the rules below are the ones the three tools share, with their common extensions.

## Rules

- **A line**: `KEY=VALUE`, `export` before it allowed, blanks around `=` and at the line's ends passed over; blank
  lines and lines starting with `#` are comments; a line ends with `\n` or `\r\n`. A key is a letter or `_`, then
  letters, digits, `_` and `.`.
- **Values**: unquoted, to the end of the line, trimmed, a `#` after a blank starting a comment (`A=a#b` is `a#b`);
  `'single quotes'`, literal, over several lines if they need; `"double quotes"`, over several lines, with the
  escapes `\n`, `\r`, `\t`, `\\`, `\"`, `\$` (any other backslash kept as written). After a closing quote only
  blanks and a comment.
- **Expansion** in unquoted and double-quoted values: `$NAME`, `${NAME}`, `${NAME:-default}` (unset or empty),
  `${NAME-default}` (unset), `${NAME:+other}` and `${NAME+other}` (the other when set), `${NAME:?message}` and
  `${NAME?message}` (an error with the message when unset); the default may hold expansions itself. A name is looked
  up among the keys above it, then in the process environment; an unset name is empty. Single quotes, `\$`, and a `$`
  before anything but a name or `{` keep the `$`. [dotenv::options](../dotenv-options.md) turns expansion and the
  environment off.
- **A key given twice**: the last value, in the first one's place (all three tools take the last).
- **The limit**: the values together, their expansions made, at most `max_size` (16 MiB): lines that double a value
  (`A=$A$A`) stop at once rather than fill the memory.
- **The oracle**: bash, which reads the part of `.env` that shell and `.env` agree on (plain values, quotes,
  comments, the expansions) as the same variables; the rest — escapes, blanks around `=`, a comment's blank, `\r\n`,
  keys with dots — is pinned by tests from the tools' documentation.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `member` | a key and its value: [dotenv::member](../dotenv-member.md) |
| `options` | what a parse does: [dotenv::options](../dotenv-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](dotenv.md) | no entries |
| [from](from.md) | of entries made in the program (static) |
| [parse, async_parse](parse.md) | the entries of a text or a stream (static) |
| [to_string](to_string.md) | the entries as a file's text |
| [load, async_load](load.md) | the entries of a file (static) |
| [save, async_save](save.md) | the entries into a file |
| [apply](apply.md) | the entries into the process environment |

#### Lookups

| Function | Description |
|---|---|
| [get](get.md) | a key's value |
| [get_int, get_double, get_bool](get_int.md) | a key's value read as a number or a boolean |
| [contains](contains.md) | whether there is the key |
| [size](size.md) | the entries |
| [empty](empty.md) | whether there is none |
| [members](members.md) | the entries in order |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | with a key set |
| [erase](erase.md) | without a key |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same entries in the same order |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::dotenv::options o;
    o.use_environment = false;
    encoding::dotenv env = encoding::dotenv::parse(R"(# the service
export HOST=example.com
PORT=8080
DEBUG=yes
GREETING="hello\tworld"
URL=http://${HOST}:${PORT}/
)", o).value();
    println("{}:{} debug {}", env.get("HOST", "localhost"), env.get_int("PORT", 80), env.get_bool("DEBUG", false));
    println(env.get("URL", "?"));
    print(env.set("PORT", "9090").to_string());
}
```

Output:

```text
example.com:8080 debug true
http://example.com:8080/
HOST=example.com
PORT=9090
DEBUG=yes
GREETING="hello\tworld"
URL=http://example.com:8080/
```

## See also

- [ini](../ini/README.md), [toml](../toml/README.md): configuration of more structure
- [io::command](../../io/command/README.md): a child's environment
- [sgcl::encoding](../README.md)
