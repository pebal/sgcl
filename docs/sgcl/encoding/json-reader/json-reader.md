[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [reader](README.md)

# sgcl::encoding::json::reader::reader

```cpp
explicit reader(const string& text) noexcept;               // (1)
reader(const string& text, const options& o) noexcept;      // (2)
explicit reader(const io::reader& in) noexcept;             // (3)
reader(const io::reader& in, const options& o) noexcept;    // (4)
reader(const reader&) = delete;                             // (5)
```

Constructs a reader. Nothing is read yet: the first token is scanned by the first call that asks for it.

- (1–2) A reader of a text in memory. The reader holds the string, which is immutable, and reads it where it lies:
  no block, no copy, and no call of it ever waits.
- (3–4) A reader of a stream, read a block at a time on the thread that calls a method, or in a task by the
  `async_` forms. Anything with a `read` is a stream: a file (`encoding::json::reader r(io::open("events.json"));`),
  a connection, an [io::buffer](../../io/buffer/README.md), a decoder of the module, a callable.
- (1), (3) With the default [options](../json-options.md), Go's v2's.
- (5) A reader is neither copied nor moved: two readers would share one position in one stream.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the JSON text: one value, or several one after another |
| `in` | the stream the text is read from |
| `o` | what the reader accepts: the depth, duplicate keys, invalid UTF-8, unknown fields of a typed read, the longest token of a stream |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = R"({"a": 1, "a": 2})";
    encoding::json::reader strict(text);
    println("{}", strict.read().has_value());
    println(strict.last_error()->message());

    encoding::json::options o;
    o.allow_duplicate_keys = true;
    encoding::json::reader lax(text, o);
    println(lax.read()->to_string());

    io::buffer in("[1, 2] [3]");
    encoding::json::reader stream(in);
    while (stream.more()) {
        println(stream.read()->to_string());
    }
}
```

Output:

```text
false
1:10: duplicate key "a"
{"a":2}
[1,2]
[3]
```

## See also

- [json::options](../json-options.md): what a reader accepts
- [json::parse](../json/parse.md): one value of a text or a stream at once
- [io streams](../../io/README.md)
- [sgcl::encoding::json::reader](README.md)
