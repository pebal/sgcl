[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [reader](../xml-reader.md)

# sgcl::encoding::xml::reader::next, async_next

```cpp
optional<token> next();                                // (1)
async::task<optional<token>> async_next() noexcept;    // (2)
```

The next [token](../xml-token.md) of the document, every one: the XML declaration, the DOCTYPE, the comments
whatever the options, the white space between elements. A token [peek](peek.md) looked at is given now.

1. Reads the stream on the thread that calls it, when the token is not in the buffer yet.
2. The same in a task: `co_await r.async_next()` gives the worker back while the stream waits.

`nullopt` at the end of the document and on an error, which [last_error](last_error.md) then keeps; after either,
every call gives `nullopt`. Go's `Decoder.Token()` returns the error with each call instead.

## Parameters

None.

## Return value

The token, or `nullopt` at the end or on an error.

## Complexity

Linear in the length of the token, amortized: each byte of the input is looked at a bounded number of times.

## Exceptions

- (1) `length_error` when a string the reader makes would pass `string::max_size()`: a document in an encoding
  other than UTF-8 is turned into UTF-8, a token is one string. What the read of the stream throws.
- (2) None: what (1) throws, the task's `co_await` or `wait()` throws again.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> count(string path) {
    encoding::xml::reader r(io::open(path).value());
    int starts = 0;
    while (auto t = co_await r.async_next()) {
        if (t->type() == encoding::xml::token::kind::start_element) {
            ++starts;
        }
    }
    println("{} elements, error: {}", starts, r.last_error().has_value());
}

int main() {
    io::write_file("tree.xml", "<a><b/><c><d/></c></a>").value();
    count("tree.xml").wait();

    encoding::xml::reader r("<a>&bad;</a>");
    while (auto t = r.next()) {
        println(int(t->type()));
    }
    println(r.last_error()->message());
}
```

Output:

```text
4 elements, error: false
0
1:4 /a: undefined entity &bad; (only the five of XML are known: no DTD is read)
```

## See also

- [peek](peek.md): the next token, left where it is
- [read](read.md): the next node whole
- [token](../xml-token.md)
- [sgcl::encoding::xml::reader](../xml-reader.md)
