[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [reader](../json-reader.md)

# sgcl::encoding::json::reader::skip, async_skip

```cpp
/*(1)*/ bool skip();
/*(2)*/ async::task<bool> async_skip() noexcept;
```

Checks the next value and passes over it: a scalar, or an array or an object to its closing bracket, every token
inside checked as [next](next.md) checks it and none of them kept. Where a key is next, the key and its value are
passed over together. The end of an array or an object is not a value: where one is next, `skip()` is an error.
Go's v2 `SkipValue`; a text is valid JSON, as Go's `json.Valid` says, when `skip()` passes over its value and
[more](more.md) finds nothing after it.

1. Reads on the thread that calls it.
2. The same in a task: `co_await r.async_skip()`.

## Parameters

None.

## Return value

`true` when a value was passed over; `false` at the end of the input or at an error, which
[last_error](last_error.md) tells apart.

## Complexity

Linear in the length of the value's text, each byte looked at once; nothing of the value is kept.

## Exceptions

- (1) What the read of the stream throws; none for a reader of a text.
- (2) None from the call, which makes the task; what the read of the stream throws inside it comes out of its
  `co_await`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

bool valid(const string& text) {
    encoding::json::reader r(text);
    return r.skip() && !r.more() && !r.last_error();
}

int main() {
    encoding::json::reader r(string(R"({"debug": {"trace": [1, 2]}, "port": 8080})"));
    r.next();
    r.skip();  // "debug" and its object
    auto key = r.next();
    auto port = r.next();
    println("{} {}", key->text(), port->as_int(0));

    println("{} {} {}", valid("[1, {\"a\": null}]"), valid("[1,]"), valid("1 2"));
}
```

Output:

```text
port 8080
true false false
```

## See also

- [read](read.md): the next value whole
- [more](more.md): whether another element follows
- [sgcl::encoding::json::reader](../json-reader.md)
