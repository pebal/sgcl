[sgcl](../../README.md) › [encoding](../README.md) › [json](../json/README.md) › [reader](README.md)

# sgcl::encoding::json::reader::read, async_read

```cpp
optional<json> read();                                               // (1)
async::task<optional<json>> async_read() noexcept;                   // (2)
template<class T> optional<T> read();                                // (3)
template<class T> async::task<optional<T>> async_read() noexcept;    // (4)
```

Reads the next value whole. The value is gathered in the block first, its end found by counting its brackets
outside its strings, and then parsed: a value read whole needs the memory of its text once, as it needs the
memory of what it is read into anyway. Where a key is next, the key is read, as a string. The end of an array or
an object is not a value: where one is next, the read is an error ([more](more.md) says so beforehand).

- (1–2) The value as a [json](../json/README.md), parsed as [json::parse](../json/parse.md) parses a text, with the
  reader's [options](../json-options.md): Go's `Decoder.Decode` into `any`, v2's `ReadValue`.
- (3–4) The value as a `T`, a type of the program described by its fields ([field_list](../field_list/README.md)) or any
  type a field may have, read as [json::parse\<T\>](../json/parse.md) reads it: an array of records a record at a
  time, `while (r.more()) { auto e = r.read<event>(); ... }`. A key no field has is skipped
  (`options::reject_unknown_fields`: an error); the error has the path inside the value and the line and the
  column in the whole input. Go's `Decoder.Decode` into a struct.
- (1), (3) Read on the thread that calls them.
- (2), (4) The same in a task: `co_await r.async_read()`, `co_await r.async_read<event>()`.

## Parameters

None.

## Return value

The value, or `nullopt` at the end of the input or at an error, which [last_error](last_error.md) tells apart.

## Complexity

Linear in the length of the value's text, looked at twice: once to find its end, once to parse it.

## Exceptions

- (1) What the read of the stream throws; none for a reader of a text.
- (3) The same, and what the program's code that reading a `T` calls throws: `describe`, a field's `from_text` or
  `from_json`, the constructors of `T` and of its fields.
- (2), (4) None from the call, which makes the task; what (1) and (3) throw comes out of its `co_await`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct event {
    string name;
    int ms = 0;

    void describe(encoding::field_list& f) {
        f.add("name", name);
        f.add("ms", ms);
    }
};

int main() {
    encoding::json::reader r(string("{\"source\": \"disk\", \"events\": [\n"
                                    "  {\"name\": \"open\", \"ms\": 3},\n"
                                    "  {\"name\": \"read\", \"ms\": \"slow\"}\n"
                                    "]}"));
    r.next();
    auto key = r.read();
    auto value = r.read();
    println("{} = {}", key->to_string(), value->to_string());
    r.next();  // "events"
    r.next();  // [
    while (r.more()) {
        auto e = r.read<event>();
        if (!e) {
            break;
        }
        println("{}: {} ms", e->name, e->ms);
    }
    println(r.last_error()->message());
}
```

Output:

```text
"source" = "disk"
open: 3 ms
3:26 /ms: expected an integer, found a string
```

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<void> print_all(io::reader in) {
    encoding::json::reader r(in);
    while (co_await r.async_more()) {
        auto v = co_await r.async_read();
        if (!v) {
            break;
        }
        println("{}", v->to_string());
    }
}

int main() {
    io::buffer in("{\"id\": 1}\n{\"id\": 2, \"tags\": [\"a\"]}\n");
    print_all(in).wait();
}
```

Output:

```text
{"id":1}
{"id":2,"tags":["a"]}
```

## See also

- [next](next.md): the next token
- [skip](skip.md): the next value checked and passed over
- [json::parse](../json/parse.md): one value of a text or a stream at once
- [field_list](../field_list/README.md): how a type of the program is read
- [sgcl::encoding::json::reader](README.md)
