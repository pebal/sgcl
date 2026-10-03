[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::csv

```cpp
#include "sgcl/encoding/csv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class csv;
}
```

`sgcl::encoding::csv` is CSV ([RFC 4180](https://www.rfc-editor.org/rfc/rfc4180)) as Go's `encoding/csv` reads
and writes it: records of fields, a record a line, a field in quotes when it holds the separator, a quote or a line
ending, `""` for a quote inside. The class is the format's name and is never constructed; its members are the types
that read and write the format — [reader](csv-reader.md), the records of a text or a stream one at a time;
[row](csv-row.md), a record with its fields and their places; [writer](csv-writer.md), records written into a
stream; [options](csv-options.md), the separator and the rest of Go's settings — the two functions of a text,
[parse](csv/parse.md) and [stringify](csv/stringify.md), and the two of a file, [load](csv/load.md) and
[save](csv/save.md).

On top of Go's: the header, a row asked by its column's name (`row["age"]`); the line and the column of each
field; and a record read or written as a type of the program, described once by its fields
([field_list](field_list.md)) for every format of the module. A text or a file of such records is one line each
way: `csv::parse<T>(text)` and `csv::stringify(records)`, `csv::load<T>(path)` and `csv::save(path, records)`.

## Rules

- **What is read is what Go reads.** A record ends at `'\n'`; `"\r\n"` is a line ending and its `'\r'` is
  dropped, inside quotes too (read as `'\n'`); a `'\r'` alone is a byte of its field, and one that ends the input
  is dropped. An empty line is skipped; with [options](csv-options.md)`::comment`, a line starting with it. A
  byte-order mark is a byte of the first field, as in Go.
- **The column counts code points**, where Go counts bytes: `ż,x` has `x` in column 3 here, 4 in Go. The two are
  the same for ASCII. Where Go names the place of an error, it is Go's place.
- **Written as Go writes it.** A field is quoted when it holds the separator, a quote, `'\r'` or `'\n'`, when it
  starts with a space (Go's `unicode.IsSpace`), or when it is `\.` (Postgres' end of data); a quote is doubled; a
  record ends with `'\n'`, or `"\r\n"` after [use_crlf()](csv-writer/use_crlf.md).
- **A record of one empty field is written `""`**, where Go writes an empty line — which every reader, Go's too,
  passes over, so the record would be lost between the writing and the reading. RFC 4180 allows the quotes, Go
  reads them as one empty field, and this is the one place the writer parts from Go's.
- **A record as a type.** A type with `describe(field_list&)` is read by the header's names
  ([reader::read\<T\>](csv-reader/read.md)) and written with a header of its field names first
  ([writer::write](csv-writer/write.md)). A field is what one text holds: a number (JSON's grammar: `12`, `-3.5`,
  `1e3`), a boolean (read as Go's `strconv.ParseBool` reads it: `1`, `t`, `T`, `TRUE`, `true`, `True` and their
  `false` forms; written `true`, `false`), a string, an enum with `names`, a type with `to_text`/`from_text`, an
  optional of one (an empty field is `nullopt`); NaN and the infinities are written `NaN`, `+Inf`, `-Inf`, as
  `strconv` writes them, and read back as `strconv.ParseFloat` reads them. Any other kind is `unsupported_value`.
- **Nothing in the format throws on its input**: a mistake in the text stops the reader with an
  [error](error.md) in [last_error()](csv-reader/last_error.md), a value with no text stops the writer's
  [flush()](csv-writer/flush.md). What throws is a mistake in the program: a separator or a comment character the
  format cannot have (`invalid_argument`, [options](csv-options.md)).
- **The oracle**: the reader and the writer are tested against Go's `encoding/csv`: named texts with their
  fields' places and errors, a hundred thousand random texts, whole and in pieces, and the text Go's writer writes.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `csv.Reader`, `csv.Writer` | [csv::reader](csv-reader.md), [csv::writer](csv-writer.md): the same text, with the header, the places in code points and records as types |
| gocsv (outside the standard library) | [read_header()](csv-reader/read_header.md), `row["name"]`, [read\<T\>()](csv-reader/read.md), [write(record)](csv-writer/write.md), [parse](csv/parse.md), [stringify](csv/stringify.md), [load](csv/load.md), [save](csv/save.md): by the fields of `describe` |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](error.md) |
| [options](csv-options.md) | the separator, the comment, Go's other settings and the bound of a record |
| [row](csv-row.md) | a record: its fields, their places, the header's names |
| [reader](csv-reader.md) | the records of a text or a stream, one at a time |
| [writer](csv-writer.md) | records written into a stream |

## Member functions

| Function | Description |
|---|---|
| `(constructor)` | deleted: the class is the name of the format |

#### Text

| Function | Description |
|---|---|
| [parse](csv/parse.md) | the records of a text, as rows or as values of a type (static) |
| [stringify](csv/stringify.md) | the text of records, a header first for values of a type (static) |

#### Files

| Function | Description |
|---|---|
| [load, async_load](csv/load.md) | the records of a file as values of a type (static) |
| [save, async_save](csv/save.md) | values into a file, a header and a record each (static) |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct item {
    string name;
    int count = 0;
    optional<double> price;

    void describe(encoding::field_list& f) {
        f.add("name", name).required();
        f.add("count", count);
        f.add("price", price);
    }
};

int main() {
    string text = "name,count,price\n"
                  "pen,3,1.5\n"
                  "\"cup, blue\",1,\n"
                  "\"note\n(two lines)\",2,0.25\n";

    encoding::csv::reader rows(text);
    rows.read_header();
    for (auto row : rows.rows()) {
        auto [line, column] = row.position(1);
        println("{} at {}:{}", row["count"].value(), line, column);
    }

    encoding::csv::reader typed(text);
    while (auto i = typed.read<item>()) {
        println("{} x{} {}", i->name, i->count, i->price.has_value());
    }

    encoding::csv::writer out(io::stdout);
    out.write({"a", "b, c", "say \"hi\""}).write(item{"pencil", 2, 0.5});
    out.flush().value();

    encoding::csv::reader bad("a,b\nc,\"d\"e\n");
    while (bad.next()) {
    }
    println("{}", bad.last_error()->message());
}
```

Output:

```text
3 at 2:5
1 at 3:13
2 at 5:14
pen x3 true
cup, blue x1 false
note
(two lines) x2 true
a,"b, c","say ""hi"""
name,count,price
pencil,2,0.5
2:5: extraneous or missing " in a quoted field
```

## See also

- [field_list](field_list.md): a type of the program described by its fields
- [json](json.md), [xml](xml.md): the other formats of records
- [io::reader](../io/reader.md), [io::writer](../io/writer.md): the streams under a reader and a writer
- [sgcl::encoding](README.md)
