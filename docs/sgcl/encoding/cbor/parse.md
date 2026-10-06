[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::parse, async_parse

```cpp
static expected<cbor, error> parse(const slice<const byte>& bytes) noexcept;                      // (1)
static expected<cbor, error> parse(const slice<const byte>& bytes, const options& o) noexcept;    // (2)
static expected<cbor, error> parse(const io::reader& in);                                         // (3)
static expected<cbor, error> parse(const io::reader& in, const options& o);                       // (4)
static async::task<expected<cbor, error>> async_parse(io::reader in) noexcept;                    // (5)
static async::task<expected<cbor, error>> async_parse(io::reader in, options o) noexcept;         // (6)
```

One value: well-formed (§3), its text strings UTF-8 and its maps without a key given twice (§5.3.1, §5.6), nested
to `max_depth`. A count or a length past the input is refused before anything is allocated, so bytes from outside
cannot make the reading take what they claim. The indefinite lengths are read; the value holds no trace of them.

1. Of the bytes, and nothing after them, with the default [options](../cbor-options.md).
2. The same with the options given.
3. One item of a stream, and no byte past it: a CBOR sequence (RFC 8742) is read an item at a time. The heads are
   read a byte at a time, so a socket goes through a [buffered_reader](../../io/buffered_reader/README.md); an item
   longer than `max_size` is refused before it is held. A stream that ends before an item is `unexpected_end` at
   offset 0.
4. The same with the options given.
5. (3) for a task, each read awaited.
6. (4) for a task.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the encoding |
| `in` | the stream |
| `o` | what is accepted |

## Return value

The value, or the [error](../error/README.md) with the offset: `unexpected_end` for an argument, a string or a
container past the input; `syntax` for the reserved additional information 28 to 30, a break where nothing of
indefinite length ends, a chunk of another type in a string of indefinite length, a simple value under 32 in two
bytes, bytes after the value; `invalid_utf8`; `duplicate_key`; `depth_limit`; `limit_exceeded` for an item of a
stream past `max_size`; `io` for a stream that failed.

## Complexity

Linear in the size of the input.

## Exceptions

- (1–2), (5–6) None.
- (3–4) What the read of the stream throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> bytes = encoding::hex::decode("a26161016162820203").value();
    auto c = encoding::cbor::parse(bytes);
    println(c->to_string());
    vector<byte> twice = encoding::hex::decode("a2616101616102").value();
    println(encoding::cbor::parse(twice).error().message());
}
```

Output:

```text
{"a": 1, "b": [2, 3]}
offset 0: a key given twice in a map: "a"
```

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // indefinite lengths, read and gone
    vector<byte> streamed = encoding::hex::decode("bf6346756ef563416d7421ff").value();
    auto c = encoding::cbor::parse(streamed).value();
    println("{} -> {}", c.to_string(), encoding::hex::encode(c.to_bytes()));
}
```

Output:

```text
{"Fun": true, "Amt": -2} -> a26346756ef563416d7421
```

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a CBOR sequence: three items, read one at a time
    vector<byte> sequence = encoding::hex::decode("0161618201f5").value();
    io::buffer stream(sequence);
    for (;;) {
        auto item = encoding::cbor::parse(io::reader(stream));
        if (!item) {
            println(item.error().message());
            break;
        }
        println(item->to_string());
    }
}
```

Output:

```text
1
"a"
[1, true]
offset 0: no item: the stream ended
```

## See also

- [to_bytes](to_bytes.md)
- [cbor::options](../cbor-options.md)
- [sgcl::encoding::cbor](README.md)
