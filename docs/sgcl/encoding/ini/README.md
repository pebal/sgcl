[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::ini

```cpp
#include "sgcl/encoding/ini.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class ini;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::ini` is the sections of an INI file and their keys and values, in the file's order, immutable, one
word shared by copying. INI has no standard; this is the INI of Python's `configparser` — the one most programs
read — with interpolation off, keys kept in their case and its strict checks, and two differences stated below.
[parse](parse.md) and [load](load.md) read it, [to_string](to_string.md) and [save](save.md) write it, [get](get.md)
and its typed forms look a value up by its section and key.

## Rules

- **Sections**: `[name]` headers; the name is the text between the `[` and the line's last `]`, as written (blanks
  and case kept). After the `]` only blanks or a comment (configparser passes over whatever follows). Keys before the
  first header belong to the section `""` (configparser refuses them).
- **Entries**: `key = value` or `key: value`, split at the first `=` or `:`; key and value trimmed of white space
  (Python's ASCII white space, as configparser's: space, tab, carriage return, form feed, vertical tab, 0x1C to 0x1F);
  an empty value allowed. A value continues on the following lines indented deeper than its key, joined with a line break; a
  blank line or a comment ends it.
- **Comments**: lines whose first non-blank character is `;` or `#`. Nothing is a comment inside a value: `a = 1 ; x`
  is the value `1 ; x`.
- **What is refused**: a section or a key in a section given twice, a line of a key without `=` or `:`, a value
  without its key (each allowed by [ini::options](../ini-options.md)); invalid UTF-8; a null character. Errors have
  their line and column.
- **Lookups** are exact: `Server` is not `server`. A line ends with `\n` or `\r\n`.
- **The oracle**: configparser reads the edge cases of every construct and 300 documents this writer wrote of random
  sections as the same sections; the writer's text reads back to the same sections and writes the same text.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `member` | a key and its value: [ini::member](../ini-member.md) |
| `section` | a section's name and its entries: [ini::section](../ini-section.md) |
| `options` | what a parse accepts: [ini::options](../ini-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](ini.md) | no sections |
| [parse, async_parse](parse.md) | the sections of a text or a stream (static) |
| [to_string](to_string.md) | the sections as a file's text |
| [load, async_load](load.md) | the sections of a file (static) |
| [save, async_save](save.md) | the sections into a file |

#### Lookups

| Function | Description |
|---|---|
| [get](get.md) | a key's value in a section |
| [get_int, get_double, get_bool](get_int.md) | a key's value read as a number or a boolean |
| [contains](contains.md) | whether there is the section, or the key in it |
| [sections](sections.md) | the sections in order |
| [size](size.md) | the sections |
| [empty](empty.md) | whether there is none |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | with a key set in a section |
| [erase](erase.md) | without a key, or a section |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | the same sections and entries in the same order |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::ini config = encoding::ini::parse(R"(name = app

; the server
[server]
host = example.com
port: 8080
tls = on
motd = Welcome!
    Second line.

[paths]
data = /var/lib/app
)").value();
    println("{}:{} tls {}", config.get("server", "host", "localhost"), config.get_int("server", "port", 80),
            config.get_bool("server", "tls", false));
    println(config.get("server", "motd", ""));
    print(config.set("server", "port", "9090").erase("paths").to_string());
}
```

Output:

```text
example.com:8080 tls true
Welcome!
Second line.
name = app

[server]
host = example.com
port = 9090
tls = on
motd = Welcome!
    Second line.
```

## See also

- [dotenv](../dotenv/README.md), [toml](../toml/README.md), [yaml](../yaml/README.md)
- [sgcl::encoding](../README.md)
