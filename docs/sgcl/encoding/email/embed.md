[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::embed

```cpp
expected<string, io::error> embed(const string& path);                 // (1)
string embed(const string& filename, const slice<const byte>& data,    // (2)
             const string& content_type = string());
```

A part the HTML shows by its Content-ID, `<img src="cid:...">`: Content-Disposition inline, a Content-ID of random
letters at the domain of From, the HTML put in multipart/related with it.

1. The file at `path`, read now.
2. Bytes of a name, of the type given or by the name's extension.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the file |
| `filename` | its name |
| `data` | its bytes |
| `content_type` | its media type; empty: by the name |

## Return value

The Content-ID, without its angle brackets, for the HTML; (1) or the `io::error` of a file that cannot be read.

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
    encoding::email m("a@example.com", "b@example.com", "Logo", "text");
    vector<byte> png(16, byte(0x89));
    string cid = m.embed("logo.png", png);
    m.set_html(string::concat("<img src=\"cid:", cid, "\">"));
    auto alt = m.body().parts()[1];
    println("{} {}", alt.content_type(), alt.parts()[1].content_id() == cid);
}
```

Output:

```text
multipart/related true
```

## See also

- [attach](attach.md)
- [set_html](set_html.md)
- [email](README.md)
