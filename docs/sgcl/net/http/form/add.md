[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::add

```cpp
form& add(const string& name, const string& value) noexcept;    // (1)
form& add(const part& p) noexcept;                              // (2)
```

Adds a part after the others, Go's `WriteField` and `CreateFormFile`.

1. A field of `name` and the text `value`.
2. The part `p`: a field, or a file ([file](file.md)).

A form is a handle, so its copies see the part too.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |
| `value` | the field's text |
| `p` | the part |

## Return value

`*this`.

## Complexity

Amortized constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::write_file("a.txt", "text");
    net::http::form f;
    f.add("user", "ann");
    f.add(net::http::form::file("attachment", "a.txt"));
    vector<byte> body = f.reader().value().read_all().value();
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), f.boundary());
    while (auto p = parts.next()) {
        if (!*p) {
            break;
        }
        println("{} '{}' {}", (*p)->name, (*p)->filename, parts.read_all()->size());
    }
}
```

Output:

```text
user '' 3
attachment 'a.txt' 4
```

## See also

- [file](file.md): a file's part
- [form](README.md)
