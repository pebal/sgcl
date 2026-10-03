[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [reader](../json-reader.md)

# sgcl::encoding::json::reader::more, async_more

```cpp
bool more();                                // (1)
async::task<bool> async_more() noexcept;    // (2)
```

Checks whether the array or the object open has another element: whether what comes next is not its end. At the
top level, whether anything but white space is left: another value of a log of values. It only looks: the
element is read by what comes next ([next](next.md), [read](read.md), [skip](skip.md)), and a second `more()`
gives the same answer. Go's `Decoder.More`.

1. Reads on the thread that calls it, as much of the stream as it takes to see the next character.
2. The same in a task: `co_await r.async_more()`.

## Parameters

None.

## Return value

`true` when another element, or another value at the top level, follows; `false` at the end of the array or the
object, at the end of the input, and after an error.

## Complexity

Linear in the white space before the next character, and a read of the stream for each block it takes.

## Exceptions

- (1) What the read of the stream throws; none for a reader of a text.
- (2) None from the call, which makes the task; what the read of the stream throws inside it comes out of its
  `co_await`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::reader r(string(R"({"sizes": [3, 1, 2], "name": "box"} {"sizes": []})"));
    while (r.more()) {
        r.next();  // {
        r.next();  // "sizes"
        r.next();  // [
        int64_t total = 0;
        while (r.more()) {
            total += r.next()->as_int(0);
        }
        r.next();  // ]
        println("{} {}", total, r.more());
        while (r.more()) {
            r.skip();
        }
        r.next();  // }
    }
    println("{}", r.more());
}
```

Output:

```text
6 true
0 false
false
```

## See also

- [next](next.md), [read](read.md), [skip](skip.md): what reads the element
- [depth](depth.md): the arrays and objects open
- [sgcl::encoding::json::reader](../json-reader.md)
