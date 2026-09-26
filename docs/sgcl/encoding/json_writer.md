# sgcl::encoding::json::writer

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class json::writer;   // JSON written piece by piece into a stream
}
```

JSON written piece by piece into an [`io::writer`](../io/stream.md): `begin_object`, `key`, `value`, `end_object`... chained, the text gathered in the writer and handed to the stream by `flush()`. What Go's `json.Encoder` and v2's `jsontext.Encoder` are, for output that is made as it goes rather than built as a [`json`](json.md) first.

## Rules

- **The structure is checked as it is written**: an end with no beginning, the end of an array where an object is open, a key outside an object, two keys in a row, a value where a key belongs, an object closed after a key with no value, NaN or an infinity (`unsupported_value`). The first mistake is kept, nothing after it is written, and `flush()` reports it — an `io::error` whose code is in the encoding category (`encoding::errc::syntax`, `unsupported_value`) — rather than every step returning something to check.
- **A value at the top level ends with a line ending**, as Go's `Encoder` writes it: a writer's values one after another are NDJSON.
- **What is not flushed is held in memory.** A long stream of values is flushed in the loop; `flush()` writes on the thread that calls it, `co_await w.async_flush()` in a task, and every other method only adds to the text.
- **The text is written as [`json::to_string`](json.md) writes it**: the numbers with their shortest digits laid out as ECMAScript does, a float with a float's digits, the strings with the escapes they need (and `<`, `>`, `&` escaped with `style::escape_html`), compact or indented by `style::indent`, as Go's `MarshalIndent` indents.
- **A value of a type of the program** ([`field_list`](fields.md)) is written by its fields, as `json::stringify` writes it; a value that has no text (NaN, a cycle) is the writer's mistake, its path in the words (`json: /1: NaN is not a JSON number`).
- **A failure of the stream is kept for good**: every later `flush()` reports it.

## Members

```cpp
class json::writer {
public:
    explicit writer(const io::writer& out);
    writer(const io::writer& out, const json::style& s);

    writer& begin_object();
    writer& end_object();
    writer& begin_array();
    writer& end_array();
    writer& key(const string& name);
    writer& value(std::nullptr_t);
    writer& value(const char* text);
    template<class T> writer& value(const T& v);   // a json, a bool, a number, a text; a type of fields.md

    expected<void, io::error> flush();                      // the text to the stream; the first mistake or failure
    task<expected<void, io::error>> async_flush();
};
```

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;

int main() {
    encoding::json::writer out(io::stdout, encoding::json::pretty);
    out.begin_object().key("count").value(3).key("squares").begin_array();
    for (auto i : range(3)) {
        out.value(i * i);
    }
    out.end_array().key("ratio").value(0.1f).end_object();
    out.flush();

    // a mistake is reported by flush
    encoding::json::writer wrong(io::stdout);
    wrong.begin_array().key("x");
    auto r = wrong.flush();
    io::stdout.write(r.error().message() + "\n");
}
```

Output:

```text
{
  "count": 3,
  "squares": [
    0,
    1,
    4
  ],
  "ratio": 0.1
}
json: a key outside an object: syntax error
```

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `json.NewEncoder(w)`, `Encode(v)` | `json::writer(out)`, `value(v)`, `flush()` | a line ending after each value at the top level |
| `SetIndent("", "  ")` | `json::writer(out, json::pretty)` | |
| `SetEscapeHTML(true)` (v1's default) | `style::escape_html` | off by default, as in v2 |
| v2 `jsontext.Encoder.WriteToken` | `begin_object`, `key`, `value`... | the structure checked, the mistake reported by `flush()` |

## See also

[`json`](json.md), the value; [`json::reader`](json_reader.md); [io streams](../io/stream.md).
