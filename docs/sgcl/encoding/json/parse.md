[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::parse, async_parse

```cpp
static expected<json, error> parse(const string& text) noexcept;                              // (1)
static expected<json, error> parse(const string& text, const options& o) noexcept;            // (2)
static expected<json, error> parse(const io::reader& in);                                     // (3)
static expected<json, error> parse(const io::reader& in, const options& o);                   // (4)
static async::task<expected<json, error>> async_parse(const io::reader& in) noexcept;         // (5)
static async::task<expected<json, error>> async_parse(io::reader in, options o) noexcept;     // (6)
template<class T> static expected<T, error> parse(const string& text);                        // (7)
template<class T> static expected<T, error> parse(const string& text, const options& o);      // (8)
template<class T> static expected<T, error> parse(const io::reader& in);                      // (9)
template<class T> static expected<T, error> parse(const io::reader& in, const options& o);    // (10)
template<class T>
static async::task<expected<T, error>> async_parse(const io::reader& in) noexcept;            // (11)
template<class T>
static async::task<expected<T, error>> async_parse(io::reader in, options o) noexcept;        // (12)
```

The one value of a text or of a stream, with nothing but white space around it: Go's `json.Unmarshal`.

- (1–2) The value of `text`, as a tree.
- (3–4) The value of the stream `in`, read to its end on the thread that calls: the whole stream is one value.
- (5–6) The same in a task, `co_await json::async_parse(in)`: the stream is read with `co_await`, and the worker
  is free while it waits.
- (7–12) The same as a program's `T`: a type described by its fields ([field_list](../field_list/README.md)) or any kind
  a field may have, `json::parse<user>(text)`. The members are read into the fields by their names; a key no
  field has is skipped, unless `options::reject_unknown_fields`; a field that is not there keeps its value,
  unless it is `required()`. An integer field takes a number whose value is an integer, `1.0` and `1e2` among
  them, which Go's v2 refuses. No tree is made on the way.

What a parse accepts is RFC 8259 with the defaults of Go's v2: invalid UTF-8, a lone surrogate (`"\ud800"`) and a
key given twice in one object are errors, and arrays and objects nest at most 512 deep;
[options](../json-options.md) change each.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |
| `in` | the stream the text is read from, to its end |
| `o` | what the parse accepts; the defaults when not given |

## Return value

The value, or an [error](../error/README.md) that says why and where: the code ([errc](../errc.md)), the byte of the
input, the line and the column — `1:13: invalid character ']' where a value was expected`, the same words and
place from a text and from a stream holding it. A number out of a double's range (`1e400`) is `out_of_range`, a
key given twice `duplicate_key`, nesting past `max_depth` `depth_limit`, a text that ends early or holds no value
`unexpected_end`, anything but white space after the value `syntax` (`1:10: invalid character 'x' after the
value`), a stream that fails `io` with the stream's [io error](../../io/error/README.md) inside. (7–12) add the
path of the value that failed as a JSON Pointer: `1:51 /manager/age: expected an integer, found a string`,
`type_mismatch`; `missing_field`, `unknown_field`, and `out_of_range` for a number a field cannot hold.

## Complexity

Linear in the length of the text.

## Exceptions

- (1–2) None.
- (3–4) What a read of `in` throws.
- (5–6), (11–12) None from the call; awaiting the task throws what (3–4) or (9–10) throw.
- (7–10) What the program's code that the reading calls throws: the constructors of `T` and of its fields,
  `describe`, a field's `from_text` or `from_json`; (9–10) and what a read of `in` throws.

## Notes

The keys of one parse are made once: a thousand objects with the same fields share each key's string, and a
thread keeps the keys of up to 256 members of at most 32 bytes from one parse to the next.

`T` needs a default constructor: the value is made first and its fields read into it.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct user {
    string name;
    int age = 0;

    void describe(encoding::field_list& f) {
        f.add("name", name).required();
        f.add("age", age);
    }
};

int main() {
    encoding::json doc = encoding::json::parse(R"({"name": "Ala", "id": 123456789012345678901})");
    println("{} {}", doc["name"].as_string("?"), doc["id"].to_string());

    println(encoding::json::parse("[1, 2,]").error().message());
    println(encoding::json::parse(R"({"a": 1, "a": 2})").error().message());
    encoding::json::options lenient;
    lenient.allow_duplicate_keys = true;
    println(encoding::json::parse(R"({"a": 1, "a": 2})", lenient).value().to_string());

    auto u = encoding::json::parse<user>(R"({"name": "Ola", "age": 1e1, "city": "Kraków"})");
    println("{} {}", u->name, u->age);
    println(encoding::json::parse<user>(R"({"name": "Ola", "age": "old"})").error().message());
    println(encoding::json::parse<user>(R"({"age": 3})").error().message());
}
```

Output:

```text
Ala 123456789012345678901
1:7: invalid character ']' where a value was expected
1:10: duplicate key "a"
{"a":2}
Ola 10
1:24 /age: expected an integer, found a string
1:10 /name: missing field
```

A stream, read in a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> program() {
    io::write_file("numbers.json", "[1, 2, 3]\n");
    auto file = io::open("numbers.json").value();
    auto numbers = co_await encoding::json::async_parse<vector<int>>(file);
    println("{}", numbers.value());
    co_return 0;
}

int main() {
    return async::run(program());
}
```

Output:

```text
[1, 2, 3]
```

## See also

- [load](load.md): the value of a file, in one call
- [to_string](to_string.md), [stringify](stringify.md): the way back to a text
- [reader](../json-reader/README.md): a text read a piece at a time
- [sgcl::encoding::json](README.md)
