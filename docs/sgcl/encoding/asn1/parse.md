[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::parse, async_parse

```cpp
static expected<asn1, error> parse(const slice<const byte>& bytes) noexcept;                      // (1)
static expected<asn1, error> parse(const slice<const byte>& bytes, const options& o) noexcept;    // (2)
static expected<asn1, error> parse(const io::reader& in);                                         // (3)
static expected<asn1, error> parse(const io::reader& in, const options& o);                       // (4)
static async::task<expected<asn1, error>> async_parse(io::reader in) noexcept;                    // (5)
static async::task<expected<asn1, error>> async_parse(io::reader in, options o) noexcept;         // (6)
```

Exactly one element: its structure and the content of every universal type it holds are checked once, so a
walk of it after cannot fail. Of bytes, the element is a view into them, which it keeps alive (a
[vector](../../core/vector/README.md), a [string](../../core/string/README.md), a file read); an input of BER with
an indefinite length or a string in pieces is written again in DER's definite form, into a buffer of its own.

1. DER of the bytes: what X.690 §10–11 allows, and nothing else.
2. As the [options](../asn1-options.md) say: `asn1::ber` for BER, a `max_depth` of one's own.
3. One element of a stream, and no byte past it — an LDAP message, a SNMP packet: the header is read a byte at a
   time, so a socket goes through a [buffered_reader](../../io/buffered_reader/README.md); then the content as one
   read, and with BER an indefinite length's elements up to their end. A declared length past `max_size` is refused
   before anything is held, so a peer cannot make the reading allocate what it claims.
4. The same as the options say.
5. (3) for a task, each read awaited.
6. (4) for a task.

## Parameters

| Parameter | Description |
|---|---|
| `bytes` | the encoding: a vector, a string's bytes, a slice of a buffer |
| `in` | the stream |
| `o` | what is accepted, and `max_size` |

## Return value

The element, or the [error](../error/README.md) with the offset of the element or the byte that broke the
rule:

- `unexpected_end`: no byte, a header or a content past the end of the input or of the element that holds it, an
  indefinite length without its end; for a stream, at offset 0 when it ended before the element (the end of an
  LDAP connection);
- `syntax`: a header DER does not allow (a length not in its shortest form, the indefinite length), a primitive
  SEQUENCE, a constructed INTEGER, a string in pieces in DER, the tag 0 where no end-of-contents may be, bytes after
  the element, a content its type does not allow (a BOOLEAN of `01`, an INTEGER not in its shortest form, a BIT
  STRING whose unused bits are set, a time not in DER's form);
- `invalid_utf8`, `invalid_character`: a string outside its character set, at the byte;
- `out_of_range`: a tag number past 2^28, a length of more than eight bytes;
- `depth_limit`: elements nested deeper than `max_depth`;
- `limit_exceeded`: an element of a stream longer than `max_size`;
- `io`: the stream failed, its [io::error](../../io/error/README.md) beside.

## Complexity

Linear in the size of the input.

## Exceptions

- (1–2), (5–6) None.
- (3–4) What the stream's read throws.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> der = encoding::hex::decode("3006020105010100").value();
    auto e = encoding::asn1::parse(der);
    if (e) {
        print(e->to_string());
    } else {
        println(e.error().message());
    }
}
```

Output:

```text
SEQUENCE
  INTEGER 5
  BOOLEAN false
```

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // BER: an indefinite length, an OCTET STRING in two pieces
    vector<byte> ber = encoding::hex::decode("308024800402616204016300000201050000").value();
    println(encoding::asn1::parse(ber).error().message());
    auto e = encoding::asn1::parse(ber, encoding::asn1::ber);
    println(encoding::hex::encode(e->bytes()));
    print(e->to_string());
}
```

Output:

```text
offset 0: an indefinite length, which DER does not allow
30080403616263020105
SEQUENCE
  OCTET STRING (3 bytes) 616263
  INTEGER 5
```

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // two LDAP messages back to back, each read alone
    vector<byte> wire = encoding::hex::decode("300c020101600702010304008000" "30050201024200").value();
    io::buffer stream(wire);
    for (;;) {
        auto message = encoding::asn1::parse(io::reader(stream), encoding::asn1::ber);
        if (!message) {
            println(message.error().message());
            break;
        }
        println("message {}: [APPLICATION {}]", *(*message)[0].as_int(), (*message)[1].tag());
    }
}
```

Output:

```text
message 1: [APPLICATION 0]
message 2: [APPLICATION 2]
offset 0: no element: the stream ended
```

## See also

- [asn1::options](../asn1-options.md)
- [bytes](bytes.md): the element's DER
- [sgcl::encoding::asn1](README.md)
