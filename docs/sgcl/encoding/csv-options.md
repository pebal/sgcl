[sgcl](../README.md) › [encoding](README.md) › [csv](csv/README.md)

# sgcl::encoding::csv::options

```cpp
#include "sgcl/encoding/csv.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class csv {
    public:
        struct options;
    };
}
```

`sgcl::encoding::csv::options` are the settings of a [reader](csv-reader/README.md) and a [writer](csv-writer/README.md), given to
their constructors: Go's `csv.Reader` fields under names of their own, and the bound of a record a reader of a
stream holds. A writer reads `separator` and `comment` alone. A program sets the ones it needs by name:
`encoding::csv::reader r(text, {.separator = ';', .comment = '#'})`.

## Rules

- The separator is not a quote, `'\r'`, `'\n'`, NUL or a byte past ASCII, and the comment character, when there is
  one, is not the separator, a quote, a line ending or a byte past ASCII: the constructor of a reader or a writer
  given other throws `invalid_argument`.
- `max_record_size` bounds what a reader of a stream holds of one record: the text of its fields. Every record of
  a stream is checked, the same however the stream hands its bytes out — one longer than the bound is
  `out_of_range` at its line, also when it came whole in one read, and a record still being gathered is refused as
  soon as it passes the bound. A reader of a text has the whole text already and never checks it.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `Comma` | `separator` |
| `Comment` | `comment`, 0 for none |
| `LazyQuotes` | `lazy_quotes` |
| `TrimLeadingSpace` | `trim_leading_space` |
| `FieldsPerRecord` 0, -1 | `same_field_count` `true`, `false`; a count given ahead (`FieldsPerRecord` > 0) has no counterpart |
| — | `max_record_size`: Go holds a record of any length |

## Member objects

| Member | Description |
|---|---|
| `char separator = ','` | the byte between the fields |
| `char comment = 0` | a line starting with it is skipped by a reader, and a writer quotes a record's first field starting with it; 0: none |
| `bool trim_leading_space = false` | a reader drops the white space at a field's start, as Go's `unicode.IsSpace` says: the ASCII spaces, U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F, U+3000 |
| `bool lazy_quotes = false` | a quote inside an unquoted field is a byte of it, and so is a quote in a quoted field not followed by the separator or a line ending; a quoted field the input ends in is a field and not an error |
| `bool same_field_count = true` | every record as long as the first, or the reader stops with `field_count`; `false` takes records of any length |
| `size_t max_record_size = size_t(16) << 20` | the longest record of a stream, the text of its fields: past it the reader stops with `errc::out_of_range` |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "# a comment\n"
                  "a;  b;c\n"
                  "d;e\"f\n";
    encoding::csv::reader r(text, {.separator = ';', .comment = '#', .trim_leading_space = true,
                                   .lazy_quotes = true, .same_field_count = false});
    while (auto row = r.next()) {
        for (auto field : *row) {
            print("[{}]", field);
        }
        println();
    }

    try {
        encoding::csv::reader wrong(text, {.separator = '"'});
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
[a][b][c]
[d][e"f]
sgcl::encoding::csv: the separator is a quote, a line ending, NUL or not ASCII
```

## See also

- [reader](csv-reader/csv-reader.md), [writer](csv-writer/csv-writer.md): the constructors that take the options
- [sgcl::encoding::csv](csv/README.md)
