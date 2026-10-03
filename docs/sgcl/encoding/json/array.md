[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::array

```cpp
static json array(std::initializer_list<json> elements) noexcept;                     // (1)
template<class R>
    requires std::ranges::input_range<const R&> &&
             std::is_convertible_v<std::ranges::range_reference_t<const R&>, json>
static json array(const R& elements);                                                 // (2)
```

An array of the given elements, in their order.

1. The elements of a list, each made by a [constructor](json.md) of its own: `json::array({1, "two", 3.0})`.
2. The elements of any range whose elements convert to a json: a [vector](../../core/vector/README.md) of numbers, of
   strings, of json values.

`array({})` is the empty array, `[]`.

## Parameters

| Parameter | Description |
|---|---|
| `elements` | the elements of the array |

## Return value

The array.

## Complexity

Linear in the number of elements.

## Exceptions

- (1) None.
- (2) What the iteration of `elements` throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto mixed = encoding::json::array({1, "two", 3.5, nullptr, encoding::json::array({})});
    println(mixed.to_string());

    vector<string> names = {"Ala", "Ola"};
    println(encoding::json::array(names).to_string());
    println(encoding::json::array(range(4)).to_string());
}
```

Output:

```text
[1,"two",3.5,null,[]]
["Ala","Ola"]
[0,1,2,3]
```

## See also

- [object](object.md): an object of members
- [builder](../json-builder/README.md): an array made in a loop
- [push_back](push_back.md): the array with one more element
- [sgcl::encoding::json](README.md)
