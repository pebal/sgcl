# sgcl::encoding::csv

```cpp
#include "sgcl/encoding/csv.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class csv;            // the format: its options and its types
    class csv::row;       // a record: its fields, their places, the header's names
    class csv::reader;    // records read from a text or a stream
    class csv::writer;    // records written into a stream
}
```

CSV ([RFC 4180](https://www.rfc-editor.org/rfc/rfc4180)) as Go's `encoding/csv` reads and writes it: records of fields, a record a line, a field in quotes when it holds the separator, a quote or a line ending, `""` for a quote inside. On top of Go's: the header — a row asked by its column's name (`row["age"]`) — the line and the column of each field, and a record read or written as a type of the program ([`field_list`](fields.md)).

## Rules

- **What is read is what Go reads.** A record ends at `'\n'`; `"\r\n"` is a line ending and its `'\r'` is dropped, inside quotes too (read as `'\n'`); a `'\r'` alone is a byte of its field, and one that ends the input is dropped. An empty line is skipped; with `options::comment`, a line starting with it. The options are Go's: `separator` (Go's `Comma`), `comment`, `lazy_quotes` (a quote inside an unquoted field is a byte of it, and a quote in a quoted field not followed by the separator or a line ending is one too), `trim_leading_space` (the white space at a field's start dropped, as Go's `unicode.IsSpace` says), `same_field_count` (Go's `FieldsPerRecord` 0: every record as long as the first; `false` is Go's -1). A byte-order mark is a byte of the first field, as in Go.
- **The first mistake stops the reader**: `next()` returns `nullopt`, as it does at the end, and `last_error()` says which — a quote in a field without quotes (`syntax`), a character after a closing quote (`syntax`), a quoted field not closed (`unexpected_end`), a record of another length (`field_count`; Go gives that record with the error, the reader here stops) — with the line and the column. Where Go names a place, it is Go's place.
- **The column counts code points**, where Go counts bytes: `ż,x` has `x` in column 3 here, 4 in Go. The two are the same for ASCII.
- **A record cut by the end of a block goes on where it stopped**: its fields gathered so far are kept and no byte is read twice, so a field of a megabyte trickling in a byte at a time costs what the megabyte costs.
- **A row is a string of its fields and their bounds**: two allocations a record, whatever the count of fields, and a row kept is valid on its own after the reader has gone on (`row[i]` is a slice of the row's own string).
- **The header**: `read_header()` takes the next record as the names of the columns; `row[name]` is the field of that column, `nullopt` when there is none. `read<T>()` takes the first record as the header when `read_header()` was not called, and fills the fields of `T` by the columns' names — a column no field has is skipped, a field whose column is not there keeps its value (`required()`: `missing_field`). A field is what one text holds: a number (JSON's grammar: `12`, `-3.5`, `1e3`), a boolean (Go's `strconv.ParseBool`: `true`, `1`, `T`, `false`...), a string, an enum with `names`, a type with `to_text`/`from_text`, an optional of one (an empty field is `nullopt`); any other kind is `unsupported_value`.
- **Written as Go writes it.** A field is quoted when it holds the separator, a quote, `'\r'` or `'\n'`, when it starts with a space (Go's `unicode.IsSpace`), or when it is `\.` (Postgres' end of data); a quote is doubled; a record ends with `'\n'`, or `"\r\n"` after `use_crlf()`, which also writes a `'\n'` inside quotes as `"\r\n"`. A record of a type of the program writes the header of its field names first; NaN and the infinities are `NaN`, `+Inf`, `-Inf`, as `strconv` writes them.
- **Every method that may reach into the stream does it on the thread that calls it**; a task uses the `async_` forms: `co_await r.async_next()`, `co_await w.async_flush()`. The writer gathers the text and `flush()` hands it to the stream; a long stream of records is flushed in the loop.

## Members

```cpp
class csv {
public:
    using error = encoding::error;
    struct options {
        char separator = ',';
        char comment = 0;                  // 0: none
        bool trim_leading_space = false;
        bool lazy_quotes = false;
        bool same_field_count = true;
        size_t max_record_size = 16 << 20;   // what a reader of a stream holds at once, a record: errc::out_of_range past it
    };
    class row;
    class reader;
    class writer;
};

class csv::row {
public:
    size_t size() const noexcept;
    bool empty() const noexcept;
    slice<const char> operator[](size_t index) const;                    // index < size(), unchecked as a vector's
    slice<const char> at(size_t index) const;                            // out_of_range past size()
    optional<slice<const char>> operator[](const string& column) const;  // by the header's name
    uint32_t line() const noexcept;                                      // the line the record starts on
    pair<uint32_t, uint32_t> position(size_t index) const noexcept;      // the field's line and column
    iterator begin() const noexcept;                                     // the fields, as slices
    iterator end() const noexcept;
};

class csv::reader {
public:
    explicit reader(const string& text);
    reader(const string& text, const csv::options& o);
    explicit reader(const io::reader& in);
    reader(const io::reader& in, const csv::options& o);   // invalid_argument: a separator that is a quote, a line ending, NUL

    optional<csv::row> read_header();
    optional<csv::row> next();
    generator<csv::row> rows();                   // for (auto row : r.rows())
    template<class T> optional<T> read();        // the next record as a T, by the header's names
    task<optional<csv::row>> async_read_header();
    task<optional<csv::row>> async_next();
    template<class T> task<optional<T>> async_read();
    const optional<encoding::error>& last_error() const noexcept;
    slice<const string> header() const noexcept;
    uint32_t line() const noexcept;               // the line the next record starts on
};

class csv::writer {
public:
    explicit writer(const io::writer& out);
    writer(const io::writer& out, const csv::options& o);
    writer& use_crlf() noexcept;
    writer& write(std::initializer_list<string> fields);
    writer& write(const csv::row& r);
    writer& write(const R& fields);               // any range of texts
    writer& write(const T& record);               // a type of fields.md; the header first
    expected<void, io::error> flush();
    task<expected<void, io::error>> async_flush();
};
```

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;
using encoding::csv;

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

    csv::reader rows(text);
    rows.read_header();
    for (auto row : rows.rows()) {
        auto [line, column] = row.position(0);
        io::stdout.write(string(row["name"].value()) + " at " + to_string(line) + ":" + to_string(column) + "\n");
    }

    csv::reader typed(text);
    while (auto i = typed.read<item>()) {
        io::stdout.write(i->name + " x" + to_string(i->count) + (i->price ? " for " + to_string(*i->price) : string()) + "\n");
    }

    csv::writer out(io::stdout);
    out.write({"a", "b, c", "say \"hi\""}).write(item{"pencil", 2, 0.5});
    out.flush().value();

    csv::reader bad(string("a,b\nc,\"d\"e\n"));
    while (bad.next()) {
    }
    io::stdout.write(bad.last_error()->message() + "\n");
}
```

Output:

```text
pen at 2:1
cup, blue at 3:1
note
(two lines) at 4:1
pen x3 for 1.500000
cup, blue x1
note
(two lines) x2 for 0.250000
a,"b, c","say ""hi"""
name,count,price
pencil,2,0.5
2:5: extraneous or missing " in a quoted field
```

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `csv.NewReader(r)`, `Read`, `ReadAll` | `csv::reader(in)`, `next()`, `rows()` | a record is a `row`: one string and the bounds of its fields |
| `Comma`, `Comment`, `LazyQuotes`, `TrimLeadingSpace`, `FieldsPerRecord` | `options::separator`, `comment`, `lazy_quotes`, `trim_leading_space`, `same_field_count` | `FieldsPerRecord` > 0 has no counterpart |
| `FieldPos`, `InputOffset` | `row.position(i)`, `row.line()` | the column in code points |
| `ReuseRecord` | — | a row is its own and may be kept |
| `ParseError`, `ErrBareQuote`, `ErrQuote`, `ErrFieldCount` | `last_error()`: `syntax`, `unexpected_end`, `field_count`, the line and the column | a record of another length stops the reader |
| — (gocsv, outside the standard library) | `read_header()`, `row["name"]`, `read<T>()`, `write(record)` | by the fields of `describe` |
| `csv.NewWriter(w)`, `Write`, `WriteAll`, `Flush`, `UseCRLF` | `csv::writer(out)`, `write`, `flush()`, `use_crlf()` | the same text |

## See also

[`field_list`](fields.md), the types of the program; [`json`](json.md); [`error`](error.md); [io streams](../io/stream.md).
