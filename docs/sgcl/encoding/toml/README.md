[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::toml

```cpp
#include "sgcl/encoding/toml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class toml;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::toml` is one value of [TOML v1.0.0](https://toml.io/en/v1.0.0), immutable, as
[json](../json/README.md) is one JSON value: 24 bytes, a pointer, a word and a kind, shared by copying. A document is
a table; a value is a string, an integer, a float, a boolean, one of the four dates and times, an array or a table.
A number, a boolean, a date and a time keep their text as it was written; the dates and times are read through
[sgcl::time](../../time/README.md) — an offset date-time as a [datetime](../../time/datetime/README.md), a local date as a
[date](../../time/date/README.md), a local time as a [duration](../../core/duration/README.md) since midnight.
[parse](parse.md) reads a document and [to_string](to_string.md) writes one. Go's standard library has no TOML;
what a Go program reads with `github.com/BurntSushi/toml` into a `map[string]any`, this reads into a value.

## Rules

- **What is read**: every rule of TOML 1.0 — bare, quoted and dotted keys; basic and literal strings, one-line and
  multi-line, with every escape and the line-ending backslash; integers in decimal, hexadecimal, octal and binary
  with underscores; floats with `inf` and `nan`; offset date-times, local date-times, local dates and local times
  (`T` or a space between the date and the time, a fraction of any length cut to the nanosecond); arrays over several
  lines with comments and a trailing comma; inline tables; `[table]` and `[[array of tables]]` headers. Errors have
  their line and column.
- **What is refused**: a key or a table defined twice; a dotted key or a header extending an inline table, a value
  or a table a header defined; `[[x]]` over an array written as a value; an integer with a leading zero or past 64
  bits (`out_of_range`); a date the calendar has not (February 30); a control character but tab; invalid UTF-8; a
  newline or a trailing comma in an inline table (TOML 1.1's drafts take them; 1.0 does not).
- **The limit** ([toml::options](../toml-options.md)): tables and arrays nested deeper than `max_depth` (512).
- **What is written**: the root table's values, then each table as `[a.b]` and each array of tables as `[[a.b]]`
  (a table holding nothing but tables gets no header of its own); keys bare when they may be, quoted otherwise;
  strings in double quotes with escapes; other arrays and tables inside arrays inline. What was read keeps its text,
  so `0xFF` is written `0xFF` and `1979-05-27 07:32:00Z` as it was. Comments and the order of a reader's whitespace
  are not kept.
- **The oracle**: Python's `tomllib` reads the spec's examples, the edge cases of every construct (valid and not),
  and 400 documents this writer wrote of random values as the same values; the writer's text reads back to an equal
  value and writes the same text. Where the two differ, this reading follows TOML and RFC 3339: the year 0000 and a
  leap second (`23:59:60`) are dates and times here (Python's `datetime` has neither), and an integer past 64 bits is
  an error here (TOML 1.0: one that cannot be kept losslessly must be refused; Python keeps it).

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `kind` | the kind of a value: [toml::kind](../toml-kind.md) |
| `member` | a key and its value: [toml::member](../toml-member.md) |
| `options` | what a parse accepts: [toml::options](../toml-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](toml.md) | an empty table, a boolean, an integer, a float, a string, a date-time, a date |
| [parse, async_parse](parse.md) | a document of a text or a stream (static) |
| [to_string](to_string.md) | the table as a document |
| [load, async_load](load.md) | the document of a file (static) |
| [save, async_save](save.md) | the table into a file |

#### Making one

| Function | Description |
|---|---|
| [local_time, local_datetime](local_time.md) | a local time, a local date-time (static) |
| [array](array.md) | an array (static) |
| [table](table.md) | a table (static) |
| [from_json](from_json.md) | a JSON value as TOML (static) |
| [to_json](to_json.md) | the value as JSON |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | the kind |
| [is_table](is_table.md) | whether it is a table |
| [is_array](is_array.md) | whether it is an array |
| [is_string](is_string.md) | whether it is a string |
| [is_integer](is_integer.md) | whether it is an integer |
| [is_floating](is_floating.md) | whether it is a float |
| [is_number](is_number.md) | whether it is an integer or a float |
| [is_bool](is_bool.md) | whether it is a boolean |
| [is_datetime](is_datetime.md) | whether it is a date or a time |
| [as_bool](as_bool.md) | a boolean |
| [as_int](as_int.md) | an integer |
| [as_double](as_double.md) | a float, or an integer rounded |
| [as_string](as_string.md) | a string |
| [as_datetime](as_datetime.md) | an offset date-time, or a local one in a zone |
| [as_date](as_date.md) | the date of a date or a date-time |
| [as_time](as_time.md) | the time of a time or a date-time |
| [text](text.md) | a scalar as written |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | an element by its index, a value by its key |
| [contains](contains.md) | whether a table has a key |
| [size](size.md) | the elements or members |
| [empty](empty.md) | whether there is none |
| [elements](elements.md) | an array's elements |
| [members](members.md) | a table's members |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | with a key set, or an element |
| [erase](erase.md) | without a key |
| [push_back](push_back.md) | with an element added |

#### Conversions

| Function | Description |
|---|---|
| [hash](hash.md) | a hash, equal for equal values |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | deep equality by the values read |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::toml config = encoding::toml::parse(R"(
title = "app"

[server]
host = "example.com"
port = 8080
started = 2026-10-06T09:30:00+02:00

[[route]]
path = "/a"

[[route]]
path = "/b"
)").value();
    println("{}:{}", config["server"]["host"].as_string("?"), config["server"]["port"].as_int(80));
    println(config["server"]["started"].as_datetime()->utc().to_string());
    for (const auto& route : config["route"].elements()) {
        println(route["path"].as_string("?"));
    }
    print(config.set("debug", true).to_string());
}
```

Output:

```text
example.com:8080
2026-10-06T07:30:00Z
/a
/b
title = "app"
debug = true

[server]
host = "example.com"
port = 8080
started = 2026-10-06T09:30:00+02:00

[[route]]
path = "/a"

[[route]]
path = "/b"
```

## See also

- [json](../json/README.md): the JSON value; [to_json](to_json.md), [from_json](from_json.md)
- [yaml](../yaml/README.md)
- [sgcl::time](../../time/README.md)
- [sgcl::encoding](../README.md)
