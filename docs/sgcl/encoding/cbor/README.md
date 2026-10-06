[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::cbor

```cpp
#include "sgcl/encoding/cbor.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class cbor;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::cbor` is one value of the Concise Binary Object Representation
([RFC 8949](https://www.rfc-editor.org/rfc/rfc8949)), immutable, as [json](../json/README.md) is one JSON value: 24
bytes, a pointer, a word and a kind, shared by copying. CBOR is JSON's data model in bytes and wider: integers to
2^64 either side, byte strings, keys of any kind, tags that say what a value means (a time, a bignum, a decimal
fraction), simple values; what COSE, WebAuthn, CWT and many devices speak. Its constructors take what C++ has
literals for, its static functions make the rest, [parse](parse.md) reads bytes and [to_bytes](to_bytes.md) writes
them, in the preferred serialization or the deterministic one; [to_string](to_string.md) is the diagnostic
notation of §8. The same value is [MessagePack](../msgpack/README.md)'s. Go's standard library has no CBOR.

## Rules

- **What parse takes**: one well-formed value (§3) and nothing after it; a text string of valid UTF-8 and a map
  with every key once (§5.3.1, §5.6), unless [options](../cbor-options.md) allow them; arrays, maps and tags nested
  to `max_depth`. A count or a length past the input is refused before anything is allocated. The indefinite
  lengths are read and leave no trace in the value.
- **What to_bytes writes**: the preferred serialization (§4.1): definite lengths, every argument in its shortest
  form, a float in the shortest of half, single and double that holds it exactly, NaN as `f97e00`. With
  [cbor::deterministic](../cbor-style.md) every map's keys are sorted by the bytes of their encodings (§4.2.1), so
  equal values write equal bytes, what a signature over CBOR needs.
- **The value**: an integer is -2^64 to 2^64 - 1 (major types 0 and 1), a bignum past it a tag 2 or 3 over bytes,
  which [as_big_integer](as_big_integer.md) reads like an integer. A float is a double. A map keeps its members in
  their order, and two maps are equal with the same members in any order. 1 and 1.0 are different values, as CBOR
  has them.
- **Tags** are carried whatever their number; the ones with a meaning here are 0 and 1 (a time,
  [as_time](as_time.md)), 2 and 3 (a bignum), 4 (a decimal fraction, [as_decimal](as_decimal.md)) and, for
  [to_json](to_json.md), 21 to 23. Their content is checked when it is asked.
- **JSON** goes both ways by §6: [from_json](from_json.md) and [to_json](to_json.md).
- **The oracle**: every example of RFC 8949's appendix A, read, written back the same and shown in its
  diagnostic notation; the examples not in preferred form read to the value the preferred bytes write; the
  inputs of appendix F refused.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `kind` | the kind of a value: [cbor::kind](../cbor-kind.md) |
| `member` | a key and its value: [cbor::member](../cbor-member.md) |
| `options` | what a parse accepts: [cbor::options](../cbor-options.md) |
| `style` | how a value is written: [cbor::style](../cbor-style.md) |

## Member objects

| Object | Description |
|---|---|
| `preferred` | the [style](../cbor-style.md) of the preferred serialization, the default |
| `deterministic` | the [style](../cbor-style.md) of the deterministic encoding: the keys sorted |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](cbor.md) | null, a boolean, an integer, a float, a text, a big integer |
| [parse, async_parse](parse.md) | a value of bytes, or an item of a stream (static) |
| [to_bytes](to_bytes.md) | the encoding |
| [to_string](to_string.md) | the diagnostic notation |

#### Making one

| Function | Description |
|---|---|
| [undefined](undefined.md) | undefined (static) |
| [simple](simple.md) | a simple value (static) |
| [bytes](bytes.md) | a byte string (static) |
| [array](array.md) | an array (static) |
| [map](map.md) | a map (static) |
| [tagged](tagged.md) | a tag over a value (static) |
| [date_time](date_time.md) | tag 0, an instant as text (static) |
| [epoch_time](epoch_time.md) | tag 1, an instant in seconds (static) |
| [decimal](decimal.md) | tag 4, a decimal fraction (static) |
| [extension](extension.md) | a MessagePack extension (static) |
| [from_json](from_json.md) | a JSON value as CBOR (static) |
| [to_json](to_json.md) | the value as JSON |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | the kind |
| [is_null](is_null.md) | whether it is null |
| [is_bool](is_bool.md) | whether it is a boolean |
| [is_number](is_number.md) | whether it is an integer or a float |
| [is_integer](is_integer.md) | whether it is an integer |
| [is_text](is_text.md) | whether it is a text string |
| [is_bytes](is_bytes.md) | whether it is a byte string |
| [is_array](is_array.md) | whether it is an array |
| [is_map](is_map.md) | whether it is a map |
| [is_tag](is_tag.md) | whether it is a tag |
| [as_bool](as_bool.md) | a boolean |
| [as_int](as_int.md) | an integer within `int64_t` |
| [as_uint](as_uint.md) | a non-negative integer |
| [as_big_integer](as_big_integer.md) | an integer or a bignum of any size |
| [as_double](as_double.md) | a float, or an integer rounded |
| [as_string](as_string.md) | a text string |
| [as_bytes](as_bytes.md) | a byte string |
| [as_time](as_time.md) | tag 0 or 1 as an instant |
| [as_decimal](as_decimal.md) | tag 4 as a mantissa and an exponent |
| [as_simple](as_simple.md) | a simple value |
| [tag](tag.md) | a tag's number |
| [content](content.md) | a tag's content |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | an element by its index, a value by its key |
| [contains](contains.md) | whether a map has a key |
| [size](size.md) | the elements or members |
| [empty](empty.md) | whether there is none |
| [elements](elements.md) | an array's elements |
| [members](members.md) | a map's members |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | with a key set, or an element |
| [erase](erase.md) | without a key, or an element |
| [push_back](push_back.md) | with an element added |

#### Conversions

| Function | Description |
|---|---|
| [hash](hash.md) | a hash, equal for equal values |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | deep equality |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::cbor reading = encoding::cbor::map({
        {"sensor", "t1"}, {"values", encoding::cbor::array({21.5, 21.75, 22})}, {1, true}});
    auto bytes = reading.to_bytes();
    println("{} bytes: {}", bytes.size(), encoding::hex::encode(bytes));

    auto read = encoding::cbor::parse(bytes);
    if (!read) {
        println(read.error().message());
        return 1;
    }
    println(read->to_string());
    println("{} {}", *(*read)["values"][1].as_double(), *(*read)[1].as_bool());
}
```

Output:

```text
28 bytes: a36673656e736f726274316676616c75657383f94d60f94d701601f5
{"sensor": "t1", "values": [21.5, 21.75, 22], 1: true}
21.75 true
```

## See also

- [msgpack](../msgpack/README.md): the same value as MessagePack
- [json](../json/README.md): the JSON value
- [asn1](../asn1/README.md): the other binary encoding of the module
- [sgcl::encoding](../README.md)
