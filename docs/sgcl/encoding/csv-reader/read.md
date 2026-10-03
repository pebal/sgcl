[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv.md) › [reader](../csv-reader.md)

# sgcl::encoding::csv::reader::read\<T\>, async_read\<T\>

```cpp
/*(1)*/ template<class T>
        optional<T> read();
/*(2)*/ template<class T>
        async::task<optional<T>> async_read() noexcept;
```

The next record as a value of `T`, a type with `describe(field_list&)` ([field_list](../field_list.md)) and a
default constructor: each field of `T` is set from the column of the header named as the field. The first record is
taken for the header when [read_header](read_header.md) was not called. No [row](../csv-row.md) is made: the
fields are set from the text of the record, and the columns of the fields are found once for the type and kept.

- A column no field has is skipped; a field whose column is not there keeps the value `T`'s constructor gave it,
  or, when it is `required()`, the reader stops with `missing_field`.
- A field is what one text holds: a number in JSON's grammar (`12`, `-3.5`, `1e3`), a boolean as Go's
  `strconv.ParseBool` reads it (`1`, `t`, `T`, `TRUE`, `true`, `True`, and `0`, `f`, `F`, `FALSE`, `false`,
  `False`), NaN and the infinities in a floating field as `strconv.ParseFloat` reads them (`NaN`, `+Inf`, `-Inf`,
  what the writer writes, and `inf`, `infinity` with a sign or none and `nan` in either case), a string, an enum with `names` (by the name; an enum without names as a number), a type with
  `to_text`/`from_text`, an optional of one, which an empty field leaves `nullopt`.
- A text that is not a value of its field's type stops the reader with `type_mismatch`, a number past the range of
  its field's type with `out_of_range`, a field of any other kind (a container, a structure) with
  `unsupported_value`; the error has the place of the field and the path of its name, `/age`.

1. Reads on the thread that calls it.
2. The same in a task.

## Parameters

None.

## Return value

The value, or `nullopt` at the end of the input or at a mistake, which [last_error()](last_error.md) tells apart.

## Complexity

Linear in the length of the record; the first record of a type, a lookup of each field's name in the header too.

## Exceptions

- (1) What the default constructor of `T` and the read of the stream under the reader throw.
- (2) None when the task is made: what the reading throws comes out of its `co_await`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

enum class level { low, high };

struct reading {
    string sensor;
    double value = 0;
    bool ok = false;
    level alarm = level::low;
    optional<int> floor;

    void describe(encoding::field_list& f) {
        f.add("sensor", sensor).required();
        f.add("value", value);
        f.add("ok", ok);
        f.add("alarm", alarm).names({"low", "high"});
        f.add("floor", floor);
    }
};

int main() {
    encoding::csv::reader r("sensor,place,value,ok,alarm,floor\n"
                            "t1,hall,21.5,true,low,2\n"
                            "t2,roof,-3e1,F,high,\n"
                            "t3,cellar,cold,T,low,0\n");
    while (auto x = r.read<reading>()) {
        println("{} {} {} {} {}", x->sensor, x->value, x->ok, x->alarm == level::high,
                x->floor.has_value());
    }
    println("{}", r.last_error()->message());
}
```

Output:

```text
t1 21.5 true false true
t2 -30 false true false
4:11 /value: "cold" is not a number
```

## See also

- [read_header](read_header.md): the header the fields are found by
- [csv::load](../csv/load.md): every record of a file as a value
- [writer::write](../csv-writer/write.md): a value written as a record
- [field_list](../field_list.md): how a type describes its fields
- [sgcl::encoding::csv::reader](../csv-reader.md)
