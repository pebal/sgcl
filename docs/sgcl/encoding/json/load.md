[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::load, async_load

```cpp
static expected<json, error> load(const string& path);                                        // (1)
template<class T> static expected<T, error> load(const string& path);                         // (2)
static async::task<expected<json, error>> async_load(string path) noexcept;                   // (3)
template<class T> static async::task<expected<T, error>> async_load(string path) noexcept;    // (4)
```

The value of a file in one call: [parse](parse.md) of the file, read as it comes, and the file closed after.

1. The value as a tree, `json::load("config.json")`.
2. The value as a program's `T`, `json::load<config>("config.json")`, by the rules of `parse<T>`.
3. (1) in a task, on the [blocking pool](../../async/spawn_blocking.md).
4. (2) in a task, on the blocking pool.

With other [options](../json-options.md), [parse](parse.md) of a file opened by [io::open](../../io/file/README.md).

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

The value, or an [error](../error/README.md): a file that cannot be opened or read is `errc::io`, the
[io error](../../io/error/README.md) inside saying why (`io_error()`), with no place in the text (its message is
`input/output error: ` and the stream's message); a text that does not parse is `parse`'s error, with its line and
its column.

## Complexity

Linear in the size of the file.

## Exceptions

- (1–2) What a read of the file throws; (2) and what the program's code that the reading calls throws: the
  constructors of `T` and of its fields, `describe`, a field's `from_text` or `from_json`.
- (3–4) None from the call; awaiting the task throws what (1–2) throw.

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
    io::write_file("server.json", R"({"host": "example.com", "port": 443, "tls": true})");
    server s = encoding::json::load<server>("server.json");
    println("{}:{} tls {}", s.host, s.port, s.tls);

    encoding::json tree = encoding::json::load("server.json");
    println(tree["port"].as_int(0));

    auto missing = encoding::json::load("missing.json");
    println(missing.error().message());
}
```

Output:

```text
example.com:443 tls true
443
input/output error: open missing.json: No such file or directory
```

## See also

- [save](save.md): a value into a file
- [parse](parse.md): the value of a text or a stream, with options
- [sgcl::encoding::json](README.md)
