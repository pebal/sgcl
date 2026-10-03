[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::load, async_load

```cpp
static expected<xml, error> load(const string& path);                                         // (1)
template<class T> static expected<T, error> load(const string& path);                         // (2)
static async::task<expected<xml, error>> async_load(string path) noexcept;                    // (3)
template<class T> static async::task<expected<T, error>> async_load(string path) noexcept;    // (4)
```

Reads a file whole, in one line: `xml::load("feed.xml")`, `xml::load<feed>("feed.xml")`.

1. The root element of the file, read as it comes, as [parse](parse.md) of a stream reads it.
2. The root element as a value of a program's type `T`, as `parse<T>` of a stream maps it.
3. and 4. (1) and (2) in a task: the file is read on a thread of the blocking pool, and the worker is given back
   meanwhile. The path is taken by value: a task runs after the call that made it.

The file is opened, read and closed; one that does not open is `errc::io`, the error of the file system in
`io_error()`.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

The root element (1, 3), or the value of `T` (2, 4); otherwise the [error](../error.md), as [parse](parse.md)
gives it, or `errc::io` when the file does not open or its read fails, with no place in the text (its message is
`input/output error: ` and the stream's message).

## Complexity

Linear in the length of the file; (2, 4) and in the size of the tree mapped.

## Exceptions

- (1–2) What [parse](parse.md) of a stream throws.
- (3–4) None: what (1) and (2) throw, the task's `co_await` or `wait()` throws again.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct point {
    int x = 0;
    int y = 0;

    void describe(encoding::field_list& f) {
        f.add("x", x).attribute();
        f.add("y", y).attribute();
    }
};

async::task<> in_a_task() {
    auto p = co_await encoding::xml::async_load<point>("point.xml");
    println("{} {}", p->x, p->y);
}

int main() {
    io::write_file("point.xml", "<point x='3' y='4'/>\n").value();
    println(encoding::xml::load("point.xml")->to_string());
    in_a_task().wait();
    auto missing = encoding::xml::load("nowhere.xml");
    println(missing.error().code() == encoding::errc::io);
}
```

Output:

```text
<point x="3" y="4"/>
3 4
true
```

## See also

- [save](save.md): the way back
- [parse](parse.md): a document from a string or a stream
- [sgcl::encoding::xml](../xml.md)
