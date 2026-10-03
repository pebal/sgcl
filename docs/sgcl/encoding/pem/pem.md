[sgcl](../../README.md) › [encoding](../README.md) › [pem](../pem.md)

# sgcl::encoding::pem::pem

```cpp
pem(const string& type, vector<byte> bytes);                                         // (1)
pem(const string& type, vector<byte> bytes, ordered_map<string, string> headers);    // (2)
explicit pem(const string& text);                                                    // (3)
```

Constructs a block. There is no empty block: a `pem` is made of its parts, or of a text.

- (1–2) A block of the type, the bytes and, in (2), the headers in their order, each checked so that the block
  writes and reads back as it is: the type a label of RFC 7468 (printable ASCII but `-`, in words joined by one
  space or one hyphen, or nothing: `CERTIFICATE`, `X509 CRL`, `RSA PRIVATE KEY`), a header's name not empty,
  without a colon, white space or a control character, not starting with `-----`, a header's value without a
  control character (a tab inside it is fine) and without white space at either end, which a reader trims.
- (3) The first block of a text the program itself wrote — a test's key, a pinned certificate — as
  [parse](parse.md) reads it. A text from outside is parsed, and its error is a value; a literal of the program is
  constructed, and a wrong one is a mistake of the program, which throws.

Copies and moves are the implicit ones: a block is a value.

## Parameters

| Parameter | Description |
|---|---|
| `type` | the label, `"CERTIFICATE"` |
| `bytes` | the bytes, taken over |
| `headers` | the headers of RFC 1421, `Proc-Type`, `DEK-Info`, in their order, taken over |
| `text` | a text with a block |

## Complexity

- (1–2) Linear in the length of the type and of the headers.
- (3) Linear in the length of the text.

## Exceptions

- (1–2) `invalid_argument` when the type or a header is not one that reads back.
- (3) `bad_expected_access<encoding::error>` with `parse`'s error, its `what()` the error's message, when the text
  has no block or a malformed one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::pem pinned("-----BEGIN CERTIFICATE-----\n"
                         "MIIBszCCAVmgAwIBAgIU\n"
                         "-----END CERTIFICATE-----\n");
    println("{} {}", pinned.type(), pinned.bytes().size());

    encoding::pem made("MESSAGE", vector<byte>(3));
    print("{}", made.to_string());

    try {
        encoding::pem wrong("PRIVATE--KEY", vector<byte>(3));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
    try {
        encoding::pem cut("-----BEGIN CERTIFICATE-----\nMIIB\n");
    } catch (const bad_expected_access<encoding::error>& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
CERTIFICATE 15
-----BEGIN MESSAGE-----
AAAA
-----END MESSAGE-----
sgcl::pem: a type that is not a label of RFC 7468
3:1: no END line for BEGIN CERTIFICATE
```

## See also

- [parse](parse.md): a block from a text from outside
- [to_string](to_string.md): the block as text
- [sgcl::encoding::pem](../pem.md)
