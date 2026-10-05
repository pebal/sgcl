[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::attach

```cpp
expected<void, io::error> attach(const string& path);                   // (1)
email& attach(const string& filename, const slice<const byte>& data,    // (2)
              const string& content_type = string());
```

An attachment, after the others, the body put in multipart/mixed: Content-Disposition attachment with the file's
name (RFC 2231 when it is not ASCII), the content written in base64 (a small ASCII file as it is).

1. The file at `path`, read now: its name the path's last element, its type by its extension
   (application/octet-stream for one not known).
2. Bytes of a name, of the type given or, when it is empty, the type of the name's extension.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `filename` | the name it goes by |
| `data` | its bytes |
| `content_type` | its media type; empty: by the name |

## Return value

- (1) Nothing, or the `io::error` of a file that cannot be read; the message is then unchanged.
- (2) `*this`.

## Complexity

Linear in the size of the content.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "Report", "See the file.");
    vector<byte> csv = {byte('a'), byte(','), byte('b'), byte('\n')};
    m.attach("dane ż.csv", csv);
    println("{}", m.body().content_type());
    for (auto& a : m.attachments()) {
        println("{} {} {}", a.filename(), a.content_type(), a.content().size());
    }
    println("{}", m.attach("no/such/file.pdf").error().is_not_found());
}
```

Output:

```text
multipart/mixed
dane ż.csv text/csv 4
true
```

## See also

- [embed](embed.md)
- [attachments](attachments.md)
- [email](README.md)
