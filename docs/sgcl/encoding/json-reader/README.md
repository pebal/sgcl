[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md)

# sgcl::encoding::json::reader

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        class reader;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::json::reader` reads JSON piece by piece, from a text in memory or from a stream a block at a
time: the [tokens](../json-token/README.md) one after another ([next](next.md)), whether the array or the object
open has another element ([more](more.md)), the next value whole as a [json](../json/README.md) or as a type of
the program ([read](read.md)), the next value checked and passed over ([skip](skip.md)).
It is what Go's `json.Decoder` and v2's `jsontext.Decoder` are: for an input that is not wanted in memory whole —
a log of values one after another (NDJSON), an array of a million records read a record at a time — and for a
loader that wants to know where each piece of its input was. `std` has no JSON.

The reader keeps the first error and stops there: a method returns `nullopt` (or `false`) at the end of the input
and at an error alike, and [last_error](last_error.md) tells the two apart and says what went wrong
and where. [json::parse](../json/parse.md) of a stream is a reader that reads one value and checks that nothing
follows it.

## Rules

- **Every method reads on the thread that calls it**, and has a form for a task with `async_` in front:
  `r.next()`, `co_await r.async_next()`. A reader of a text in memory never waits.
- **The first error stops it.** The error has the offset in the input, the line and the column (in code points),
  counted across the blocks already let go: the lines of a block are counted when the reader lets the block go,
  so a text read without an error counts nothing it does not have to. Every call after an error returns
  `nullopt` or `false`.
- **The grammar is RFC 8259's, and the defaults are Go's v2**: invalid UTF-8 and a lone surrogate (`\ud800`
  alone) are errors, and so is a key given twice in one object; [options](../json-options.md)`::allow_invalid_utf8`
  puts U+FFFD in their place, `allow_duplicate_keys` lets the key through. A byte-order mark is not JSON. Nesting
  deeper than `options::max_depth` (512) is `depth_limit`.
- **Several values at the top level follow one another**, with or without white space between them (`1 2[3]{}`),
  as Go's `Decoder` reads them: a reader of NDJSON reads a value a line.
- **A token cut by the end of a block is not a case of its own.** The scan of a string, a number or a word stops
  where the data ends and goes on from there when the next block comes; the part of a string already decoded is
  kept, so a long string trickling in a byte at a time is scanned once. A block too small for a token grows: 8 KB
  (`config::io_buffer_size`), then twice the size, and so on up to `options::max_token_size` (64 MB), past which
  a token or a value read whole is `out_of_range`: memory a stream from the network asks for is bounded.
- **The text of a token is a slice of the reader's memory**, which the reader writes over: a text kept past the
  next call is copied ([token::text](../json-token/text.md)).
- **A value read whole is gathered in the block first**, its end found by counting its brackets outside its
  strings, and then parsed as [json::parse](../json/parse.md) parses a text: a value read whole needs the memory of
  its text once, as it needs the memory of the tree anyway.
- **A reader is neither copied nor moved.** It holds the text or the stream (an `io::reader`, which holds what it was
  made of), the block and the brackets open.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `json.NewDecoder(r)`, v2 `jsontext.NewDecoder(r)` | `json::reader(in)`, `json::reader(text)`; `async_next`, `async_read`… in a task |
| `Decoder.Token`, v2 `ReadToken` | `next()`; a key is a token of its own kind, the text of a string decoded |
| `Decoder.More` | `more()` |
| `Decoder.Decode(&v)` into `any`, v2 `ReadValue` | `read()`, a [json](../json/README.md) |
| `Decoder.Decode(&s)` into a struct | `read<T>()`, the fields by [describe](../field_list/README.md) |
| v2 `SkipValue` | `skip()` |
| `InputOffset`, v2 `StackDepth` | `offset()`, `depth()` |
| `SyntaxError.Offset` | `last_error()->offset()`, and `line()` and `column()` too |
| v2 `AllowDuplicateNames`, `AllowInvalidUTF8` | `options::allow_duplicate_keys`, `allow_invalid_utf8`; the defaults are v2's |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json-reader.md) | constructs a reader of a text or of a stream |
| `(destructor)` | destroys the reader; the rest of the input is not read |

#### Reading

| Function | Description |
|---|---|
| [next, async_next](next.md) | the next token |
| [more, async_more](more.md) | checks whether the array or the object open has another element |
| [read, async_read](read.md) | the next value whole, as a json or as a type of the program |
| [skip, async_skip](skip.md) | checks the next value and passes over it |

#### Observers

| Function | Description |
|---|---|
| [last_error](last_error.md) | the error the reader stopped at |
| [offset](offset.md) | the byte of the input where the next token starts |
| [depth](depth.md) | the number of arrays and objects open |

## Complexity

`next`, `more` and `skip` look at each byte of the input once, a token cut by a block included; `read` looks at
the bytes of a value twice, once to find its end and once to parse it.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // NDJSON: one value a line
    string log = "{\"level\": \"info\", \"msg\": \"started\"}\n"
                 "{\"level\": \"warn\", \"msg\": \"slow disk\", \"ms\": 812}\n";
    encoding::json::reader lines(log);
    while (lines.more()) {
        auto entry = lines.read();
        if (!entry) {
            break;
        }
        println("{}: {}", (*entry)["level"].as_string("?"), (*entry)["msg"].as_string(""));
    }

    // The tokens of a text, with their depth
    encoding::json::reader tokens(string(R"({"id": 7, "tags": ["a", "b"]})"));
    while (auto t = tokens.next()) {
        println("{}{}", string(tokens.depth() * 2, ' '), t->text());
    }

    // An error, with its line and column
    encoding::json::reader bad("[1,\n 2,\n ]");
    while (bad.next()) {
    }
    println(bad.last_error()->message());
}
```

Output:

```text
info: started
warn: slow disk
  {
  id
  7
  tags
    [
    a
    b
  ]
}
3:2: invalid character ']' where a value was expected
```

## See also

- [json::token](../json-token/README.md): what `next` returns
- [json::writer](../json-writer/README.md): JSON written piece by piece
- [json](../json/README.md): the value; [json::parse](../json/parse.md): a whole text or stream at once
- [json::options](../json-options.md): what the reader accepts
- [field_list](../field_list/README.md): the types of the program `read<T>` reads
- [error](../error/README.md), [errc](../errc.md): what `last_error` holds
- [io streams](../../io/README.md)
- [sgcl::encoding::json](../json/README.md)
