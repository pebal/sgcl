[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::msgpack

```cpp
#include "sgcl/encoding/msgpack.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class msgpack;
}
```

`sgcl::encoding::msgpack` is MessagePack ([its specification](https://github.com/msgpack/msgpack/blob/master/spec.md)),
read into and written from the value of [cbor](../cbor/README.md), whose data model holds it: nil as null, booleans,
integers, floats, str as a text string, bin as a byte string, arrays, maps, and the extension types as a kind of
their own ([cbor::extension](../cbor/extension.md)). A value read as MessagePack can be written as CBOR and back,
looked at, changed and compared the same way. Go's standard library has no MessagePack.

## Rules

- **What parse takes**: one value in any format of the spec, nothing after it; a str of valid UTF-8 and a map with
  every key once unless the [options](../cbor-options.md) allow them (the options of cbor, the same struct);
  nested to `max_depth`. A count or a length past the input is refused before anything is allocated. The byte
  `0xC1` is no format.
- **What encode writes**: each value in its shortest format — a fixint, the narrowest of the integers, a float32
  when it holds the value exactly, the fix forms of str, array and map up to their lengths, a fixext of 1, 2, 4, 8
  or 16 bytes. A CBOR instant (tag 1) is written as the timestamp extension. What MessagePack has no format for —
  undefined, a simple value, an integer below -2^63, another tag (a bignum among them) — is `invalid_argument`.
- **The timestamp** (extension -1) is read as an extension, unchanged, and [cbor::as_time](../cbor/as_time.md)
  gives its instant, in any of its three forms; [timestamp](timestamp.md) makes one in the shortest form that
  holds the instant.
- `msgpack` is a codec, plain functions; it holds nothing.

## Member functions

| Function | Description |
|---|---|
| [parse, async_parse](parse.md) | a value of bytes, or of a stream (static) |
| [encode](encode.md) | the bytes of a value (static) |
| [timestamp](timestamp.md) | the timestamp extension of an instant (static) |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor value = encoding::cbor::map({{"compact", true}, {"schema", 0}});
    auto bytes = encoding::msgpack::encode(value);
    println("{} bytes: {}", bytes.size(), encoding::hex::encode(bytes));
    auto read = encoding::msgpack::parse(bytes);
    if (!read) {
        println(read.error().message());
        return 1;
    }
    println(read->to_string());
    println("as CBOR: {}", encoding::hex::encode(read->to_bytes()));
}
```

Output:

```text
18 bytes: 82a7636f6d70616374c3a6736368656d6100
{"compact": true, "schema": 0}
as CBOR: a267636f6d70616374f566736368656d6100
```

## See also

- [cbor](../cbor/README.md): the value, and CBOR
- [json](../json/README.md)
- [sgcl::encoding](../README.md)
