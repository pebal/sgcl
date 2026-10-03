[sgcl](../../README.md) › [encoding](../README.md) › [csv](README.md)

# sgcl::encoding::csv::load\<T\>, async_load\<T\>

```cpp
template<class T>
static expected<vector<T>, error> load(const string& path);                         // (1)
template<class T>
static async::task<expected<vector<T>, error>> async_load(string path) noexcept;    // (2)
```

The records of a file as values of `T`, the file's first line the header whose names the fields of `T` are found
by, as [reader::read\<T\>](../csv-reader/read.md) finds them: a column no field has is skipped, a field whose
column is not there keeps its value, or is `missing_field` when it is `required()`. `T` has a default constructor
and `describe(field_list&)` ([field_list](../field_list/README.md)).

1. Reads the file on the thread that calls it.
2. The same in a task: the reading runs on the [blocking pool](../../async/spawn_blocking.md), and the task waits for it
   without holding its worker. The path is taken by value, since a task is lazy and the caller's string may be gone
   before it runs.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |

## Return value

The values, one for each record after the header, in the order of the file; or the [error](../error/README.md):
`errc::io` with the stream's error in `io_error()` when the file does not open or a read fails, with no place in
the text (its message is `input/output error: ` and the stream's message); the reader's error when a record does not
read (a mistake of the text, a field that is not a value of its type), with its line and its column.

## Complexity

Linear in the size of the file.

## Exceptions

- (1) What the default constructor of `T` throws.
- (2) None when the task is made: what the reading throws comes out of its `co_await`.

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
    io::write_file("cities.csv", "name,people\nOslo,709037\nBergen,291940\n").value();
    vector<city> cities = encoding::csv::load<city>("cities.csv").value();
    for (const city& c : cities) {
        println("{}: {}", c.name, c.people);
    }

    io::write_file("cities.csv", "name,people\nOslo,709037\nBergen,many\n").value();
    println("{}", encoding::csv::load<city>("cities.csv").error().message());
    println("{}", encoding::csv::load<city>("towns.csv").error().message());
}
```

Output:

```text
Oslo: 709037
Bergen: 291940
3:8 /people: "many" is not an integer
input/output error: open towns.csv: No such file or directory
```

## See also

- [save](save.md): values into a file
- [parse](parse.md): the records of a text
- [reader::read\<T\>](../csv-reader/read.md): a record of a reader as a type
- [sgcl::encoding::csv](README.md)
