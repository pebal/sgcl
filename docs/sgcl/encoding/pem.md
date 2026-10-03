[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::pem

```cpp
#include "sgcl/encoding/pem.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class pem;
}
```

`sgcl::encoding::pem` is a block of the textual encoding of [RFC 7468](https://www.rfc-editor.org/rfc/rfc7468):
bytes — a certificate, a key, a request — as base64 between two lines that name what they are,
`-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----`, with the headers of
[RFC 1421](https://www.rfc-editor.org/rfc/rfc1421) (`Proc-Type`, `DEK-Info`) that older encrypted keys carry
between the first line and the base64. A `pem` is one block, a value: its [type](pem/type.md), its
[bytes](pem/bytes.md) and its [headers](pem/headers.md). It is Go's `pem.Block`.

A text from outside — a file, a request — is read with [parse](pem/parse.md), the first block of it, and
[parse_all](pem/parse_all.md), every one of them; both return the block or an [error](error.md) with its line and its
column. A block the program itself writes as text (a test's key, a pinned certificate) is constructed,
`encoding::pem block(text)`, which throws `parse`'s error; the [constructor](pem/pem.md) from a type and bytes, with
two or three arguments, makes a block from its parts, and [to_string](pem/to_string.md) writes it.

## Rules

- **The text around the blocks is skipped**: before the first, between them and after the last, as section 5.2
  lets a file explain itself there. A `-----BEGIN ` counts only at the start of a line.
- **A block that is there and malformed is an error, not a block passed over**: a `BEGIN` with no `END`, an `END`
  of another type, base64 that is not, a boundary line without its five dashes or with text after them, a type
  that is not a label. The error has its line and its column (`4:10: END B does not match BEGIN A`). Go's
  `pem.Decode` passes over a malformed block and goes on to the next, which hides the one a program was looking
  for.
- **The lax grammar of section 3 is read**: white space anywhere in the base64 (space, tab, the line endings,
  vertical tab, form feed), lines of any length, CRLF, white space after the dashes. The base64 itself is strict:
  bits past the data in the last character are refused, since a key or a certificate has one encoding (Go's
  `pem.Decode` takes them).
- **Headers**: when the first line after `BEGIN` has a colon, the lines are headers, `Name: value`, in their order,
  a name given twice keeping its last value. RFC 1421 ends them with an empty line, and only then may a line that
  starts with white space go on with the value before it (RFC 822's folding): a block whose headers end in an empty
  line is read so. One with no empty line after them is read as Go reads it — the headers are the lines with a
  colon, and the first line without one, indented or not, is the start of the base64 — so that an indented line of
  base64 is never taken into a header's value.
- **Writing**: [to_string()](pem/to_string.md) writes section 3's strict form — lines of 64 characters, `"\n"` at
  every end, `Proc-Type` first among the headers (RFC 1421 wants it there) and the rest in their order, an empty
  line after them — which is what Go's `EncodeToMemory` writes for the same block (Go writes the other headers
  sorted by name).
- **A block that could not be written and read back cannot be made**: a type that is not a label of the RFC
  (`CERTIFICATE`, `X509 CRL`: printable ASCII but `-`, in words joined by one space or one hyphen, or nothing), a
  header name that is empty, holds a colon, white space or a control character, or starts with `-----`, a value
  with a control character (a tab inside it is fine) or with white space at either end, which a reader trims —
  `invalid_argument`. A mistake in the program throws; a mistake in the input is an error value.
- A `pem` holds its bytes and its headers in managed containers: it lives where a `tracked_ptr` may. A private key
  read into one is in managed memory, where a secret must not be: the keys of the crypto module read their PEM
  with their own `from_pem`, into memory they zero.
- **The oracle**: the fourteen examples of RFC 7468 itself — figures 6 to 19, the non-conforming labels of
  appendix A among them — are read as the RFC prints them (the explanatory text of figure 7 included), give the type
  and the bytes Go's `pem.Decode` gives, are each one DER value of its own length, and write back as the RFC wrote
  them. Beside them, blocks Go's `EncodeToMemory` writes and texts its `Decode` reads.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `pem.Block` | `encoding::pem`: [type()](pem/type.md), [bytes()](pem/bytes.md), [headers()](pem/headers.md), the headers in the order of the text |
| `pem.Decode(data)` | [pem::parse(text)](pem/parse.md), the first block, and [parse_all](pem/parse_all.md), every block: a malformed block is an error with a line and a column, not skipped; no rest of the text is returned |
| `pem.Encode`, `pem.EncodeToMemory` | [to_string()](pem/to_string.md), `Proc-Type` first and the other headers in their order |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](error.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](pem/pem.md) | a block from a type, bytes and headers, or from a text the program writes |
| [parse](pem/parse.md) | the first block of a text (static) |
| [parse_all](pem/parse_all.md) | every block of a text (static) |

#### Observers

| Function | Description |
|---|---|
| [type](pem/type.md) | the label of the block, `"CERTIFICATE"` |
| [bytes](pem/bytes.md) | the bytes the base64 holds |
| [headers](pem/headers.md) | the headers of RFC 1421, in their order |

#### Conversions

| Function | Description |
|---|---|
| [to_string](pem/to_string.md) | the block as text |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string chain =
        "The server's certificate, then its issuer's.\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIBszCCAVmgAwIBAgIU\n"
        "-----END CERTIFICATE-----\n"
        "-----BEGIN CERTIFICATE-----\n"
        "MIIBqzCCAVGgAwIBAgIU\n"
        "-----END CERTIFICATE-----\n";
    auto blocks = encoding::pem::parse_all(chain);
    for (const auto& block : blocks.value()) {
        println("{}: {} bytes", block.type(), block.bytes().size());
    }

    auto bad = encoding::pem::parse(
        "-----BEGIN PRIVATE KEY-----\nMIIB\n-----END PUBLIC KEY-----\n");
    println("{}", bad.error().message());

    ordered_map<string, string> headers;
    headers.insert_or_assign("Comment", "made by hand");
    encoding::pem key("EC PRIVATE KEY", vector<byte>(40), headers);
    print("{}", key.to_string());
}
```

Output:

```text
CERTIFICATE: 15 bytes
CERTIFICATE: 15 bytes
3:10: END PUBLIC KEY does not match BEGIN PRIVATE KEY
-----BEGIN EC PRIVATE KEY-----
Comment: made by hand

AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==
-----END EC PRIVATE KEY-----
```

## See also

- [base64](base64.md): the encoding inside a block
- [error](error.md): why a text is not a block
- [ordered_map](../core/ordered_map.md): the headers
- [sgcl::encoding](README.md)
