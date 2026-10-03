[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md)

# sgcl::encoding::csv::stringify

```cpp
/*(1)*/ template<class R>
        static expected<string, error> stringify(const R& records);
/*(2)*/ template<class R>
        static expected<string, error> stringify(const R& records, const options& o);
```

The text of the records in one call, what a [writer](../csv-writer.md) writes of them; the form of a text that
[save](save.md) is of a file, as [json::stringify](../json/stringify.md) is of JSON. `records` is a range of what
[writer::write](../csv-writer/write.md) takes: values of a type with `describe(field_list&)`
([field_list](../field_list.md)), written with a header of their fields' names first; or [rows](../csv-row.md) and
ranges of texts, a record each and no header. Each record ends with `'\n'`.

1. The text with Go's settings: fields separated by `','`.
2. The same with the [options](../csv-options.md) `o`: a separator of `';'`, a comment character that a first field
   starting with it is quoted for.

## Parameters

| Parameter | Description |
|---|---|
| `records` | the records to write |
| `o` | the separator, the comment character and Go's other settings |

## Return value

The text, empty for no records, or an [error](../error.md) where a field has no text: `unsupported_value` with the
path of the field (a container, a record), `/list: a value CSV has no text for`. The error has no place, there
being no text it was read from.

## Complexity

Linear in the size of the text.

## Exceptions

- `length_error` when the text passes the 4 GiB a [string](../../core/string.md) holds, measured after each record.
- (2) `invalid_argument` when `o` names a separator or a comment character the format cannot have
  ([options](../csv-options.md)).
- What the program's code that the writing calls throws: `describe`, a field's `to_text`.

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
    vector<city> cities = {{"Oslo", 709037}, {"Bergen, Norway", 291940}};
    print("{}", encoding::csv::stringify(cities).value());

    vector<vector<string>> table = {{"a", "say \"hi\""}, {"b", ""}};
    encoding::csv::options semicolons;
    semicolons.separator = ';';
    print("{}", encoding::csv::stringify(table, semicolons).value());
}
```

Output:

```text
name,people
Oslo,709037
"Bergen, Norway",291940
a;"say ""hi"""
b;
```

## See also

- [parse](parse.md): the other direction
- [save](save.md): the records into a file
- [writer](../csv-writer.md): records written into a stream
- [sgcl::encoding::csv](../csv.md)
