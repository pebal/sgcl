[sgcl](../README.md) › [encoding](README.md) › [json](json.md)

# sgcl::encoding::json::writer

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        class writer;
    };
}
```

`sgcl::encoding::json::writer` writes JSON piece by piece into a stream: [begin_object](json-writer/begin_object.md),
[key](json-writer/key.md), [value](json-writer/value.md), [end_object](json-writer/end_object.md)… chained, the
text gathered in the writer and handed to the stream by [flush](json-writer/flush.md). It is what Go's
`json.Encoder` and v2's `jsontext.Encoder` are, for output that is made as it goes rather than built as a
[json](json.md) first: a log of values, an array of a million records written a record at a time. `std` has no
JSON.

The steps return nothing to check. A mistake in the structure — an end with no beginning, a key outside an object,
a value where a key belongs, NaN — is kept, and `flush()` reports it; a failure of the stream is kept the same way.
So a loop writes, and one `flush()` at its end says whether all of it went out.

## Rules

- **The structure is checked as it is written**: an end with no beginning, the end of an array where an object
  is open, a key outside an object, two keys in a row, a value where a key belongs, an object closed after a key
  with no value, NaN or an infinity. The first mistake is kept, and from then on `flush()` reports it, as an
  `io::error` whose code is in the encoding category ([errc](errc.md)`::syntax`, `unsupported_value`), and
  writes nothing: not what came after the mistake, nor what was held before it. An array or an object still open is not a mistake: `flush()` writes what there is.
- **A value at the top level ends with a line ending**, as Go's `Encoder` writes it: a writer's values one after
  another are NDJSON.
- **What is not flushed is held in memory**, and the destructor does not flush it: a long stream of values is
  flushed in the loop, and the last of it after the loop. `flush()` writes on the thread that calls it,
  `co_await w.async_flush()` in a task; every other method only adds to the text and never waits.
- **The text is written as [to_string](json/to_string.md) writes a json**: the numbers with their shortest digits
  laid out as ECMAScript does, a `float` with a float's digits, the strings with the escapes they need (and `<`,
  `>`, `&` escaped with [style](json-style.md)`::escape_html`), compact or indented by `style::indent`, as Go's
  `MarshalIndent` indents.
- **A value of a type of the program** ([field_list](field_list.md)) is written by its fields, as
  [json::stringify](json/stringify.md) writes it; a value that has no text (NaN, an enum's value past its names,
  a cycle of pointers past 512 levels) is the writer's mistake, its path in the words
  (`json: /x: NaN is not a JSON number`).
- **A failure of the stream is kept for good**: every later `flush()` reports it and writes nothing.
- **A writer is neither copied nor moved.** It holds the stream (an `io::writer`, which holds what it was made
  of) and the text, and lives where it is used: on a thread's stack or in a task's frame.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `json.NewEncoder(w)`, `Encode(v)` | `json::writer(out)`, `value(v)`, `flush()`; a line ending after each value at the top level |
| `SetIndent("", "  ")` | `json::writer(out, json::pretty)` |
| `SetEscapeHTML(true)`, v1's default | `style::escape_html`, off by default, as in v2 |
| v2 `jsontext.Encoder.WriteToken` | `begin_object`, `key`, `value`…; the structure checked, the mistake reported by `flush()` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json-writer/json-writer.md) | constructs a writer into a stream |
| `(destructor)` | destroys the writer; the text not flushed is dropped |

#### Structure

| Function | Description |
|---|---|
| [begin_object](json-writer/begin_object.md) | opens an object |
| [end_object](json-writer/end_object.md) | closes the object open |
| [begin_array](json-writer/begin_array.md) | opens an array |
| [end_array](json-writer/end_array.md) | closes the array open |
| [key](json-writer/key.md) | the key of the next member of the object open |

#### Values

| Function | Description |
|---|---|
| [value](json-writer/value.md) | writes a value: null, a text, a boolean, a number, a json, a type of the program |

#### Output

| Function | Description |
|---|---|
| [flush, async_flush](json-writer/flush.md) | hands the text to the stream; the first mistake or failure |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::writer out(io::stdout, encoding::json::pretty);
    out.begin_object().key("count").value(3).key("squares").begin_array();
    for (int i : range(3)) {
        out.value(i * i);
    }
    out.end_array().key("ratio").value(0.1f).end_object();
    out.flush();

    // a mistake is reported by flush
    encoding::json::writer wrong(io::stdout);
    wrong.begin_array().key("x");
    auto r = wrong.flush();
    println(r.error().message());
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

## See also

- [json::reader](json-reader.md): JSON read piece by piece
- [json](json.md): the value; [json::stringify](json/stringify.md): a whole value at once
- [json::style](json-style.md): how the text is laid out
- [field_list](field_list.md): the types of the program `value` writes
- [io streams](../io/README.md)
- [sgcl::encoding::json](json.md)
