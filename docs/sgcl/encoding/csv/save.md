[sgcl](../../README.md) › [encoding](../README.md) › [csv](README.md)

# sgcl::encoding::csv::save, async_save

```cpp
template<class R>
static expected<void, error> save(const string& path, const R& records);        // (1)
template<class R>
static async::task<expected<void, error>> async_save(string path, R records)    // (2)
    noexcept(std::is_nothrow_move_constructible_v<R>);
```

Writes the records into a file, made or written over: each element of `records` as
[writer::write](../csv-writer/write.md) writes it. A value of a type with `describe(field_list&)` is a record of
its fields, and the first one writes the header of their names before it; an element that is a range of texts is a
record of those texts, with no header. When the writing or the closing of the file fails, the file is removed.

1. Writes on the thread that calls it.
2. The same in a task: the writing runs on the [blocking pool](../../async/spawn_blocking.md), and the task waits for it
   without holding its worker. The path and the records are taken by value, since a task is lazy and the caller's
   may be gone before it runs.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `records` | a range of values of a type with `describe`, or of ranges of texts |

## Return value

Nothing, or the [error](../error/README.md): `errc::io` with the stream's error in `io_error()` when the file cannot be
made, the writing or the closing fails, or a field has no text in CSV (its `unsupported_value` is the code of the
stream's error). The error has no place: its message is `input/output error: ` and the stream's message.

## Complexity

Linear in the size of the text written.

## Exceptions

- (1) What `describe` of the element type throws, and the conversion of an element to text.
- (2) What the move of `records` throws; none when it is noexcept. What the writing throws comes out of the task's
  `co_await`.

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
    vector<city> cities;
    cities.push_back(city{"Oslo", 709037});
    cities.push_back(city{"Bergen, the old capital", 291940});
    encoding::csv::save("cities.csv", cities).value();
    print("{}", io::read_text("cities.csv").value());

    vector<vector<string>> table;
    table.push_back(vector<string>{"x", "y"});
    table.push_back(vector<string>{"1", "2"});
    encoding::csv::save("table.csv", table).value();
    print("{}", io::read_text("table.csv").value());
}
```

Output:

```text
name,people
Oslo,709037
"Bergen, the old capital",291940
x,y
1,2
```

## See also

- [load](load.md): the records of a file as values
- [stringify](stringify.md): the records as a text
- [writer::write](../csv-writer/write.md): a record into a stream
- [sgcl::encoding::csv](README.md)
