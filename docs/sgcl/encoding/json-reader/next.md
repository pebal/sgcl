[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md) › [reader](../json-reader.md)

# sgcl::encoding::json::reader::next, async_next

```cpp
optional<token> next();                                // (1)
async::task<optional<token>> async_next() noexcept;    // (2)
```

The next [token](../json-token.md) of the input: a bracket that opens or closes an array or an object, a key, a
string, a number, a boolean or null. The commas and colons are checked and passed over, never handed out; a key
is a token of its own kind, so a loader needs no state of its own to tell a key from a string value. A token cut
by the end of a block is gone on with when the next block comes. Go's `Decoder.Token` and v2's
`Decoder.ReadToken`.

1. Reads on the thread that calls it; a reader of a stream waits there for the stream's next block.
2. The same in a task: `co_await r.async_next()` gives the worker back while the stream has nothing to read.

## Parameters

None.

## Return value

The token, or `nullopt` at the end of the input or at an error, which [last_error](last_error.md) tells apart.

## Complexity

Linear in the bytes of the token and the white space before it, each looked at once, and a read of the stream
for each block they take.

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
    encoding::json::reader r(string(R"({"user": "ala", "roles": ["admin"], "active": true})"));
    while (auto t = r.next()) {
        if (t->type() == encoding::json::token::kind::key) {
            print("{}: ", t->text());
        } else if (t->type() == encoding::json::token::kind::string) {
            println("\"{}\"", t->text());
        } else {
            println("{}", t->text());
        }
    }
}
```

Output:

```text
{
user: "ala"
roles: [
"admin"
]
active: true
}
```

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> count_numbers(io::reader in) {
    encoding::json::reader r(in);
    int numbers = 0;
    while (auto t = co_await r.async_next()) {
        if (t->type() == encoding::json::token::kind::number) {
            ++numbers;
        }
    }
    co_return numbers;
}

int main() {
    io::buffer in("[1, [2, 3], {\"a\": 4.5, \"b\": \"6\"}]");
    println("{}", count_numbers(in).wait());
}
```

Output:

```text
4
```

## See also

- [json::token](../json-token.md): the kind and the text
- [read](read.md): the next value whole
- [skip](skip.md): the next value passed over
- [sgcl::encoding::json::reader](../json-reader.md)
