[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::save, async_save

```cpp
template<class T>
static expected<void, error> save(const string& path, const string& name, const T& value);    // (1)
template<class T>
static async::task<expected<void, error>> async_save(string path, string name, T value)       // (2)
    noexcept(std::is_nothrow_move_constructible_v<T>);
expected<void, error> save(const string& path) const;                                         // (3)
async::task<expected<void, error>> async_save(string path) const noexcept;                    // (4)
```

Writes a file whole, in one line: `xml::save("feed.xml", "feed", f)`, `doc.save("feed.xml")`. The file is made or
written over, and a new line follows the text.

1. The element `name` made of a program's `value`, as [stringify](stringify.md) writes it, compact.
2. (1) in a task: the file is written on a thread of the blocking pool, and the worker is given back meanwhile.
   The arguments are taken by value: a task runs after the call that made it.
3. This node, as [to_string](to_string.md) writes it, compact.
4. (3) in a task, the same way as (2).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |
| `name` | the name of the element the value is written as |
| `value` | the value of a program's type, described by `describe(field_list&)` |

## Return value

Nothing; otherwise the [error](../error.md): `errc::io` when the file cannot be written, the error of the file
system in `io_error()`, with no place (its message is `input/output error: ` and the stream's message), and (1–2)
what [stringify](stringify.md) fails with, `errc::unsupported_value` for a value with no form in XML.

## Complexity

Linear in the length of the text written.

## Exceptions

- (1) What [stringify](stringify.md) throws.
- (2) What the move constructor of `T` throws, when the task is made; what (1) throws, the task's `co_await` or
  `wait()` throws again.
- (3) `length_error` when the text would pass `string::max_size()`.
- (4) None: what (3) throws, the task's `co_await` or `wait()` throws again.

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

async::task<> in_a_task(encoding::xml doc) {
    auto saved = co_await doc.async_save("doc.xml");
    println(saved.has_value());
}

int main() {
    encoding::xml::save("point.xml", "point", point{3, 4}).value();
    print(io::read_all_text(io::open("point.xml").value()).value());

    in_a_task(encoding::xml("doc", "text")).wait();
    print(io::read_all_text(io::open("doc.xml").value()).value());

    auto nowhere = encoding::xml("a").save("no/such/directory/a.xml");
    println(nowhere.error().code() == encoding::errc::io);
}
```

Output:

```text
<point x="3" y="4"/>
true
<doc>text</doc>
true
```

## See also

- [load](load.md): the way back
- [to_string](to_string.md), [stringify](stringify.md): the text without a file
- [sgcl::encoding::xml](../xml.md)
