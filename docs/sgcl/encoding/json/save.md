[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::save, async_save

```cpp
template<class T> static expected<void, error> save(const string& path, const T& value);    // (1)
template<class T>
static async::task<expected<void, error>> async_save(string path, T value)                  // (2)
    noexcept(std::is_nothrow_move_constructible_v<T>);
expected<void, error> save(const string& path) const;                                       // (3)
async::task<expected<void, error>> async_save(string path) const noexcept;                  // (4)
```

A value into a file in one call: the file made or written over, the text compact and a new line after it.

1. The text of a program's value, [stringify](stringify.md)`(value)`: `json::save("config.json", cfg)`.
2. (1) in a task, on the [blocking pool](../../async/spawn_blocking.md); the task holds its own `value` and `path`, so
   it may run after the caller's objects are gone.
3. The text of this value, [to_string](to_string.md)`()`: `doc.save("config.json")`.
4. (3) in a task, on the blocking pool; the value is copied into the task.

For another style, [stringify](stringify.md)`(value, json::pretty)` or [to_string](to_string.md) with
[io::write_file](../../io/file/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |
| `value` | the value to write |

## Return value

Nothing, or an [error](../error/README.md): a file that cannot be made or written is `errc::io`, the
[io error](../../io/error/README.md) inside saying why (`io_error()`), with no place (its message is
`input/output error: ` and the stream's message); (1–2) a value with no text is the error of
[stringify](stringify.md), and nothing is written.

## Complexity

Linear in the size of the text.

## Exceptions

- (1) `length_error` when the text would pass the 4 GiB a [string](../../core/string/README.md) holds, what the program's
  code that the writing calls throws (`describe`, a field's `to_text` or `to_json`), and what a write of the file
  throws.
- (2) What the move of `T` throws; none when it is noexcept. Awaiting the task throws what (1) throws.
- (3) `length_error` when the text would pass 4 GiB, and what a write of the file throws.
- (4) None from the call; awaiting the task throws what (3) throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct server {
    string host = "localhost";
    int64_t port = 8080;
    bool tls = false;

    void describe(encoding::field_list& f) {
        f.add("host", host);
        f.add("port", port);
        f.add("tls", tls);
    }
};

int main() {
    encoding::json::save("server.json", server{"example.com", 443, true});
    server s = encoding::json::load<server>("server.json");
    println("{}:{} tls {}", s.host, s.port, s.tls);
    print("{}", io::read_text("server.json").value_or(string("?")));

    encoding::json doc = encoding::json::load("server.json");
    doc.set("port", 8443).save("server.json");
    print("{}", io::read_text("server.json").value_or(string("?")));

    println(encoding::json::save("no/such/dir/server.json", s).error().message());
}
```

Output:

```text
example.com:443 tls true
{"host":"example.com","port":443,"tls":true}
{"host":"example.com","port":8443,"tls":true}
input/output error: open no/such/dir/server.json: No such file or directory
```

## See also

- [load](load.md): a value from a file
- [stringify](stringify.md), [to_string](to_string.md): the text, for a file of another style
- [sgcl::encoding::json](README.md)
