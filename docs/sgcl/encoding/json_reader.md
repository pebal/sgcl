# sgcl::encoding::json::reader

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class json::reader;   // JSON read piece by piece: tokens, values whole, skips
    class json::token;    // a token: its kind and its text
}
```

JSON read piece by piece, from a text in memory or from a stream a block at a time: the tokens one after another (`next`), whether the array or the object open has another element (`more`), the next value whole as a [`json`](json.md) (`read`), the next value checked and passed over (`skip`). What Go's `json.Decoder` and v2's `jsontext.Decoder` are: for an input that is not wanted in memory whole — a log of values one after another (NDJSON), an array of a million records read a record at a time — and for a loader that wants to know where each piece of its input was.

## Rules

- **Every method reads on the thread that calls it**, and has a form for a task with `async_` in front: `r.next()`, `co_await r.async_next()`. A reader of a text in memory never waits.
- **The first error stops it.** A method returns `nullopt` (or `false`) at the end of the input and at an error alike; `last_error()` tells them apart and says what went wrong and where: the offset in the input, the line and the column (in code points), counted across the blocks already let go — the lines of a block are counted when the reader lets the block go, so a text that is read without an error counts nothing it does not have to. Every call after an error returns `nullopt`.
- **The grammar is RFC 8259's**, and the defaults are Go's v2: invalid UTF-8 and a lone surrogate (`\ud800` alone) are errors, and so is a key given twice in one object (`options::allow_invalid_utf8` puts U+FFFD in their place; `options::allow_duplicate_keys` lets the key through). A byte-order mark is not JSON. Nesting deeper than `options::max_depth` (512) is `depth_limit`. Several values at the top level follow one another, with or without white space between them, as Go's `Decoder` reads them.
- **A token cut by the end of a block is not a case of its own.** The scan of a string, a number or a word stops where the data ends and goes on from there when the next block comes; the part of a string already decoded is kept, so a long string trickling in a byte at a time is scanned once. A block too small for a token grows (8 KB, then twice the size, and so on).
- **The text of a token is a slice of the reader's memory**: it holds that memory, but the reader writes over it, so a text kept past the next call is copied (`string(t.text())`). A string's text has its escapes decoded; a number's is its literal; a bracket's is the bracket.
- **`read()` gathers the next value in the block first**, found by counting its brackets outside its strings, and then parses it as [`json::parse`](json.md) does — so a value read whole needs the memory of its text once, as it needs the memory of the tree anyway. Where a key is next, `read()` reads the key as a string (and `skip()` passes over the key and its value).
- **`read<T>()` reads the next value as a type of the program** ([`field_list`](fields.md)) — an array of records a record at a time: `while (r.more()) { auto e = r.read<event>(); ... }`; the error has the path inside the value and the line in the whole input.
- **`more()` only looks.** Inside an array or an object it says whether the next thing is not its end; at the top level, whether anything but white space is left.

## Members

```cpp
class json::reader {
public:
    explicit reader(const string& text);
    reader(const string& text, const json::options& o);
    explicit reader(const io::reader& in);
    reader(const io::reader& in, const json::options& o);

    optional<json::token> next();              // nullopt: the end of the input, or an error
    bool more();                               // another element in the array or object open
    optional<json> read();                     // the next value whole
    bool skip();                               // the next value checked and passed over
    template<class T> optional<T> read();      // the next value as a T (fields.md), the error with its path
    task<optional<json::token>> async_next();  // the same in a task: co_await r.async_next()
    task<bool> async_more();
    task<optional<json>> async_read();
    task<bool> async_skip();
    template<class T> task<optional<T>> async_read();

    const optional<encoding::error>& last_error() const noexcept;
    uint64_t offset() const noexcept;          // the byte where the next token starts (or its white space)
    uint32_t depth() const noexcept;           // the arrays and objects open
};

class json::token {
public:
    enum class kind : uint8_t {
        begin_object, end_object, begin_array, end_array, key, string, number, boolean, null
    };
    kind type() const noexcept;
    const slice<const char>& text() const noexcept;
    optional<bool> as_bool() const noexcept;
    optional<int64_t> as_int() const noexcept;     // a number whose value is an integer that fits: 1e2 is 100
    optional<uint64_t> as_uint() const noexcept;
    optional<double> as_double() const noexcept;   // rounded once from the literal; nullopt past a double's range
};
```

## Example

```cpp
#include "sgcl/sgcl.h"

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
        io::stdout.write((*entry)["level"].as_string().value_or("?") + ": " + (*entry)["msg"].as_string().value_or("") + "\n");
    }

    // The tokens of a text, with their depth
    encoding::json::reader tokens(string(R"({"id": 7, "tags": ["a", "b"]})"));
    while (auto t = tokens.next()) {
        io::stdout.write(string(std::string(tokens.depth() * 2, ' ')) + string(t->text()) + "\n");
    }

    // An error, with its line and column
    encoding::json::reader bad(string("[1,\n 2,\n ]"));
    while (bad.next()) {
    }
    io::stdout.write(bad.last_error()->message() + "\n");
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

A stream is read the same way, a block at a time: `encoding::json::reader r(io::open("events.json").value());`.

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `json.NewDecoder(r)`, v2 `jsontext.NewDecoder(r)` | `json::reader(in)`, `json::reader(text)` | `async_next`, `async_read`… in a task |
| `Decoder.Token`, `ReadToken` | `next()` | a key is a token of its own kind; the text of a string decoded |
| `Decoder.More` | `more()` | |
| `Decoder.Decode(&v)` into `any`, `ReadValue` | `read()` | a `json` |
| `Decoder.Decode(&s)` into a struct | `read<T>()` | |
| `SkipValue` | `skip()` | |
| `InputOffset`, `StackDepth` | `offset()`, `depth()` | |
| `SyntaxError.Offset` | `last_error()->offset()`, `line()`, `column()` | the line and the column too |
| `AllowDuplicateNames`, `AllowInvalidUTF8` | `options::allow_duplicate_keys`, `allow_invalid_utf8` | the defaults are v2's |

## See also

[`json`](json.md), the value; [`json::writer`](json_writer.md); [`error`](error.md); [io streams](../io/stream.md).
