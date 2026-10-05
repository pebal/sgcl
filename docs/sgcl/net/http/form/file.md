[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::file

```cpp
static part file(const string& name, const string& path) noexcept;                                // (1)
static part file(const string& name, const string& path, const string& content_type) noexcept;    // (2)
```

A file's part: the file at `path` sent as the field `name`, Go's `CreateFormFile` with the file copied in. Its name in
the body is the path's last element (`filename="photo.png"`). Nothing is read now: the file is opened when the body
reaches its part, and its size taken when the body is made.

1. The type by the file's extension, as the file server takes it: `image/png`, `text/plain; charset=utf-8`,
   `application/octet-stream` for an extension not known.
2. The type given.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |
| `path` | the file's path |
| `content_type` | the part's `Content-Type` |

## Return value

The part, for the form's constructor or [add](add.md).

## Complexity

Linear in the length of the path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::write_file("data.csv", "a,b\n1,2\n");
    net::http::form f{net::http::form::file("table", "data.csv", "text/csv")};
    f.add(net::http::form::file("again", "data.csv"));
    vector<byte> body = f.reader().value().read_all().value();
    string text(slice<const char>(reinterpret_cast<const char*>(body.data()), body.size()));
    for (auto& line : text.split("\r\n")) {
        if (line.starts_with("Content-")) {
            println("{}", line);
        }
    }
}
```

Output:

```text
Content-Disposition: form-data; name="table"; filename="data.csv"
Content-Type: text/csv
Content-Disposition: form-data; name="again"; filename="data.csv"
Content-Type: text/csv; charset=utf-8
```

## See also

- [add](add.md): a part after the others
- [form](README.md)
