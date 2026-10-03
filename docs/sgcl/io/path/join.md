[sgcl](../../README.md) › [io](../README.md) › [path](../path.md)

# sgcl::io::path::join

```cpp
template<class R>
    requires std::ranges::input_range<R>
          && std::convertible_to<std::ranges::range_reference_t<R>, const string&>
string join(const R& elements) noexcept(/* see below */);                             // (1)
string join(std::initializer_list<string> elements) noexcept;                         // (2)
template<class... S>
string join(const string& first, const S&... rest) noexcept(/* see below */);         // (3)
```

Joins the elements with the separator and [cleans](clean.md) the result, Go's `filepath.Join`. Empty elements are
skipped, and when every element is empty the result is empty (not `.`). An element may hold separators of its own:
`join("a", "b/../c")` is `a/c`.

1. The elements of a range of strings: a [vector](../../core/vector.md)`<string>`, a `std::vector<string>`, a list.
   `noexcept` when the range is contiguous and its elements are `string`s.
2. The elements of a braced list.
3. The elements as arguments, each after the first anything a [string](../../core/string.md) is made from: a
   string, a literal, a `std::string_view`. `noexcept` when every argument after the first is text that makes a
   string without a throw: a string, a literal or another character array, a `std::string_view`, a C string.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements to join |
| `first`, `rest` | the elements to join, in order |

## Return value

The joined and cleaned path; an empty string when every element is empty.

## Complexity

Linear in the total length of the elements.

## Exceptions

- (1) None for a contiguous range of `string`s (a `vector<string>`, a `std::vector<string>`, an array); for another
  range, what its iteration and the conversion of its elements to `const string&` throw.
- (2) None.
- (3) None for arguments of text; what making a string of an argument of another type throws.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{}", io::path::join("/usr", "local", "bin"));
    println("{}", io::path::join("a", "", "b/../c"));
    println("{}", io::path::join({string("docs"), string("sgcl"), string("README.md")}));
    vector<string> parts = {"x", "y/", "z"};
    println("{}", io::path::join(parts));
    println("\"{}\"", io::path::join("", ""));
}
```

Output:

```text
/usr/local/bin
a/c
docs/sgcl/README.md
x/y/z
""
```

## See also

- [split](split.md), [dir](dir.md), [base](base.md): a path taken apart
- [under](under.md): a name from outside joined to a directory only when it stays inside
- [sgcl::io::path](../path.md)
