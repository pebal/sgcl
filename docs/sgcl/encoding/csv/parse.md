[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md)

# sgcl::encoding::csv::parse

```cpp
/*(1)*/ static expected<vector<row>, error> parse(const string& text) noexcept;
/*(2)*/ static expected<vector<row>, error> parse(const string& text, const options& o);
/*(3)*/ template<class T>
        static expected<vector<T>, error> parse(const string& text);
/*(4)*/ template<class T>
        static expected<vector<T>, error> parse(const string& text, const options& o);
```

The records of a text in one call, what a [reader](../csv-reader.md) over it reads to its end; the form of a text
that [load](load.md) is of a file, as [json::parse](../json/parse.md) is of JSON.

1. Every record as a [row](../csv-row.md), the first among them: no header is taken, as Go's `ReadAll` takes none.
   Each row keeps its fields, their places and nothing of the others.
2. The same with the [options](../csv-options.md) `o`: a separator of `';'`, a comment character.
3. The records as values of `T`, the text's first line the header whose names the fields of `T` are found by, as
   [reader::read\<T\>](../csv-reader/read.md) finds them: a column no field has is skipped, a field whose column is
   not there keeps its value, or is `missing_field` when it is `required()`. `T` has a default constructor and
   `describe(field_list&)` ([field_list](../field_list.md)).
4. The same with the options `o`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the CSV text |
| `o` | the separator, the comment character and Go's other settings |

## Return value

The records in the order of the text, none for a text of nothing or of empty lines alone, or the
[error](../error.md) of the first record that does not read, with its line and its column: a mistake of the text
(a quote, a record of another number of fields), or a field that is not a value of its type (3–4). The records
before it are dropped.

## Complexity

Linear in the size of the text.

## Exceptions

- (1) None.
- (2), (4) `invalid_argument` when `o` names a separator or a comment character the format cannot have
  ([options](../csv-options.md)).
- (3–4) What the default constructor of `T` throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct city {
    string name;
    int people = 0;

    void describe(encoding::field_list& f) {
        f.add("name", name);
        f.add("people", people);
    }
};

int main() {
    auto rows = encoding::csv::parse("name,people\nOslo,709037\n\"Bergen, Norway\",291940\n");
    for (auto& row : *rows) {
        println("{} | {}", row[0], row[1]);
    }

    vector<city> cities = encoding::csv::parse<city>("name,people\nOslo,709037\n").value();
    for (const city& c : cities) {
        println("{}: {}", c.name, c.people);
    }

    encoding::csv::options semicolons;
    semicolons.separator = ';';
    auto european = encoding::csv::parse<city>("name;people\nKraków;804237\n", semicolons);
    println("{}", european->front().name);

    println("{}", encoding::csv::parse("a,b\n1\n").error().message());
}
```

Output:

```text
name | people
Oslo | 709037
Bergen, Norway | 291940
Oslo: 709037
Kraków
2:1: wrong number of fields: 1, the first record has 2
```

## See also

- [stringify](stringify.md): the other direction
- [load](load.md): the records of a file
- [reader](../csv-reader.md): the records one at a time, of a text or a stream
- [sgcl::encoding::csv](../csv.md)
