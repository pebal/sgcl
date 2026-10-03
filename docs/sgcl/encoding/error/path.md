[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::path

```cpp
const string& path() const noexcept;
```

Where in the structure the error was found, for a format that has one: a JSON Pointer (`/users/3/age`) in JSON, a
path of elements (`/catalog/book[2]/@id`) in XML, the field of a type (`/age`) when a CSV record is read as one.
[message()](message.md) shows it after the position.

## Parameters

None.

## Return value

The path; empty where it does not apply.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct person {
    string name;
    int age = 0;

    void describe(encoding::field_list& f) {
        f.add("name", name);
        f.add("age", age);
    }
};

int main() {
    encoding::csv::reader r("name,age\nAnn,31\nBob,old\n");
    while (r.read<person>()) {
    }
    const auto& e = r.last_error().value();
    println("[{}] {}", e.path(), e.message());

    auto block = encoding::pem::parse("no block");
    println("[{}] {}", block.error().path(), block.error().message());
}
```

Output:

```text
[/age] 3:5 /age: "old" is not an integer
[] 1:9: no PEM block
```

## See also

- [set_path](set_path.md): the path set
- [sgcl::encoding::error](../error.md)
