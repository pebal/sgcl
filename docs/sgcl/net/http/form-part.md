[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [form](form/README.md) › part

# sgcl::net::http::form::part

```cpp
#include "sgcl/net/http/form.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class form {
    public:
        class part {
        public:
            part(const string& name, const string& value) noexcept;   // a field
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::http::form::part` is one part of a [form](form/README.md): a field, made of a name and a text, or a file,
made by [form::file](form/file.md) of a name, a path and a type. Its constructor is not explicit, so a field is
written as a pair in braces, `{"name", "value"}`, in the form's constructor and its [add](form/add.md). A value: its
strings are handles, and a copy shares them.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::form::part field{"note", "hello"};
    net::http::form f{field, {"other", "x"}};
    f.add(field);
    vector<byte> body = f.reader().value().read_all().value();
    net::http::multipart_reader parts(io::reader(make_tracked<io::buffer>(body)), f.boundary());
    while (auto p = parts.next()) {
        if (!*p) {
            break;
        }
        println("{}", (*p)->name);
    }
}
```

Output:

```text
note
other
note
```

## See also

- [form::file](form/file.md): a file's part
- [form](form/README.md)
