# sgcl::encoding::pem

```cpp
#include "sgcl/encoding/pem.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class pem;   // a block: the type, the bytes, the headers
}
```

The textual encoding of [RFC 7468](https://www.rfc-editor.org/rfc/rfc7468): bytes — a certificate, a key, a request — as base64 between two lines that name what they are,

```
-----BEGIN CERTIFICATE-----
MIIBszCCAVmgAwIBAgIU...
-----END CERTIFICATE-----
```

with the headers of [RFC 1421](https://www.rfc-editor.org/rfc/rfc1421) (`Proc-Type`, `DEK-Info`) that older encrypted keys carry between the first line and the base64. A `pem` is one block, a value; `parse` reads the first block of a text and `parse_all` every one of them.

## Rules

- **The text around the blocks is skipped**: before the first, between them and after the last, as section 5.2 lets a file explain itself there. A `-----BEGIN ` counts only at the start of a line.
- **A block that is there and malformed is an error, not a block passed over**: a `BEGIN` with no `END`, an `END` of another type, base64 that is not, a boundary line without its five dashes or with text after them, a type that is not a label. The error has its line and its column (`4:10: END B does not match BEGIN A`). Go's `pem.Decode` passes over a malformed block and goes on to the next, which hides the one a program was looking for.
- **The lax grammar of section 3 is read**: white space anywhere in the base64 (space, tab, the line endings, vertical tab, form feed), lines of any length, CRLF, white space after the dashes. The base64 itself is strict: bits past the data in the last character are refused, since a key or a certificate has one encoding (Go's `pem.Decode` takes them).
- **Headers**: when the first line after `BEGIN` has a colon, the lines are headers, `Name: value`, in their order, a name given twice keeping its last value. RFC 1421 ends them with an empty line, and only then may a line that starts with white space go on with the value before it (RFC 822's folding): a block whose headers end in an empty line is read so. One with no empty line after them is read as Go reads it — the headers are the lines with a colon, and the first line without one, indented or not, is the start of the base64 — so that an indented line of base64 is never taken into a header's value.
- **Writing**: `to_string()` writes section 3's strict form — lines of 64 characters, `"\n"` at every end, `Proc-Type` first among the headers (RFC 1421 wants it there) and the rest in their order, an empty line after them — which is what Go's `EncodeToMemory` writes for the same block (Go writes the other headers sorted by name).
- **A block that could not be written and read back cannot be made**: a type that is not a label of the RFC ("CERTIFICATE", "X509 CRL", printable ASCII in words joined by one space or hyphen), a header name that is empty, holds a colon, white space or a control character, or starts with `-----`, a value with a control character (a tab inside it is fine) or with white space at either end, which a reader trims — `invalid_argument`.
- `parse` of a text with no block is `unexpected_end` at its end; `parse_all` of it is an empty vector.
- A `pem` holds its bytes and its headers in managed containers: it lives where a `tracked_ptr` may.

## Members

```cpp
class pem {
public:
    using error = encoding::error;

    encoding::pem(const string& type, vector<byte> bytes);
    encoding::pem(const string& type, vector<byte> bytes, ordered_map<string, string> headers);

    const string& type() const noexcept;                            // "CERTIFICATE"
    const vector<byte>& bytes() const noexcept;
    const ordered_map<string, string>& headers() const noexcept;   // in the order of the text
    string to_string() const;

    static expected<pem, error> parse(const string& text);              // the first block
    static expected<vector<pem>, error> parse_all(const string& text);  // every block
};
```

## Example

```cpp
#include "sgcl/encoding/pem.h"
#include "sgcl/io/os.h"

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
    for (auto& block : blocks.value()) {
        io::stdout.write(block.type() + ": " + string(std::to_string(block.bytes().size())) + " bytes\n");   // CERTIFICATE: 15 bytes
    }

    auto bad = encoding::pem::parse("-----BEGIN PRIVATE KEY-----\nMIIB\n-----END PUBLIC KEY-----\n");
    io::stdout.write(bad.error().message() + "\n");   // 3:10: END PUBLIC KEY does not match BEGIN PRIVATE KEY

    ordered_map<string, string> headers;
    headers.insert_or_assign("Comment", "made by hand");
    encoding::pem key("EC PRIVATE KEY", vector<byte>(40), headers);
    io::stdout.write(key.to_string());
}
```

```
-----BEGIN EC PRIVATE KEY-----
Comment: made by hand

AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==
-----END EC PRIVATE KEY-----
```

## The oracle

The fourteen examples of RFC 7468 itself — figures 6 to 19, the non-conforming labels of appendix A among them — are read as the RFC prints them (the explanatory text of figure 7 included), give the type and the bytes Go's `pem.Decode` gives, are each one DER value of its own length, and write back as the RFC wrote them. Beside them, blocks Go's `EncodeToMemory` writes and texts its `Decode` reads.

## See also

[base64](base64.md); [`error`](error.md); [`ordered_map`](../core/ordered_map.md).
