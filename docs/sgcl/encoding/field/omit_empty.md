[sgcl](../../README.md) › [encoding](../README.md) › [field](README.md)

# sgcl::encoding::field::omit_empty

```cpp
field& omit_empty() noexcept;
```

Marks the field to be left out when it is empty: 0, `false`, an empty string, an empty container, `nullopt`, a
null pointer, and a value of another type when it compares equal to its default value with `==` (or has an
`empty()` that says so). A described type without `==` is never empty, and so is a `std::variant`, `std::pair`
or `std::tuple` holding a type without one. JSON leaves the member out, XML the
element or the attribute; CSV writes every column. Reading is not changed: a field left out keeps its value. Go's
`omitempty` and v2's `omitzero` in one.

## Parameters

None.

## Return value

`*this`, for the next option in the chain.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct range_of_lines {
    int from = 0;
    int to = 0;

    bool operator==(const range_of_lines&) const = default;

    void describe(encoding::field_list& f) {
        f.add("from", from);
        f.add("to", to);
    }
};

struct hit {
    string file;
    int line = 0;
    vector<string> tags;
    optional<double> score;
    range_of_lines context;

    void describe(encoding::field_list& f) {
        f.add("file", file).omit_empty();
        f.add("line", line).omit_empty();
        f.add("tags", tags).omit_empty();
        f.add("score", score).omit_empty();
        f.add("context", context).omit_empty();
    }
};

int main() {
    println(encoding::json::stringify(hit{}).value());
    println(encoding::json::stringify(hit{"a.cpp", 0, {"todo"}, 0.0, {3, 9}}).value());
}
```

Output:

```text
{}
{"file":"a.cpp","tags":["todo"],"score":0,"context":{"from":3,"to":9}}
```

## See also

- [required](required.md): an absent field an error when read
- [sgcl::encoding::field](README.md)
