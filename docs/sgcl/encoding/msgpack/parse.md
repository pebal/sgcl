[sgcl](../../README.md) › [encoding](../README.md) › [msgpack](README.md)

# sgcl::encoding::msgpack::parse, async_parse

```cpp
static expected<cbor, error> parse(const slice<const byte>& bytes) noexcept;                            // (1)
static expected<cbor, error> parse(const slice<const byte>& bytes, const cbor::options& o) noexcept;    // (2)
static expected<cbor, error> parse(const io::reader& in);                                               // (3)
static expected<cbor, error> parse(const io::reader& in, const cbor::options& o);                       // (4)
static async::task<expected<cbor, error>> async_parse(io::reader in) noexcept;                          // (5)
static async::task<expected<cbor, error>> async_parse(io::reader in, cbor::options o) noexcept;         // (6)
```

One value as a [cbor](../cbor/README.md) value: every format of the spec, the longer ones where a shorter would do
among them; a str of valid UTF-8 and a map with every key once. A count or a length past the input is refused
before anything is allocated.

1. Of the bytes, and nothing after them, with the default [options](../cbor-options.md).
2. The same with the options given.
3. One value of a stream, and no byte past it: its formats are read a byte at a time, so a socket goes through a
   [buffered_reader](../../io/buffered_reader/README.md); a value longer than `max_size` is refused before it is
   held. A stream that ends before a value is `unexpected_end` at offset 0.
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

The value, or the [error](../error/README.md) with the offset: `unexpected_end` for a value, a length or a
container past the input; `syntax` for the byte `0xC1`, bytes after the value, a timestamp of a length other than
4, 8 or 12; `out_of_range` for a timestamp of more than 999999999 nanoseconds; `invalid_utf8`; `duplicate_key`;
`depth_limit`; `limit_exceeded` for a value of a stream past `max_size`; `io` for a stream that failed.

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
    vector<byte> bytes = encoding::hex::decode("83a161cd0100a162c4020102a163d6ff6553f100").value();
    auto v = encoding::msgpack::parse(bytes);
    println(v->to_string());
    println((*v)["c"].as_time()->to_string());
    println(encoding::msgpack::parse(encoding::hex::decode("c1").value()).error().message());
}
```

Output:

```text
{"a": 256, "b": h'0102', "c": extension(-1, h'6553f100')}
2023-11-14T22:13:20Z
offset 0: the byte 0xC1, which no format has
```

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // values one after another on a stream, each read alone
    vector<byte> wire = encoding::hex::decode("01a26869920203").value();
    io::buffer stream(wire);
    for (;;) {
        auto v = encoding::msgpack::parse(io::reader(stream));
        if (!v) {
            println(v.error().message());
            break;
        }
        println(v->to_string());
    }
}
```

Output:

```text
1
"hi"
[2, 3]
offset 0: no item: the stream ended
```

## See also

- [encode](encode.md)
- [sgcl::encoding::msgpack](README.md)
