[sgcl](../../README.md) › [encoding](../README.md) › [json](../json.md)

# sgcl::encoding::json::operator[]

```cpp
/*(1)*/ const json& operator[](const string& key) const noexcept;
/*(2)*/ template<size_t N> const json& operator[](const char (&key)[N]) const noexcept;
/*(3)*/ const json& operator[](size_t index) const noexcept;
```

A member or an element, read only, and null when there is none: a key that is not there, an index past the end, a
member asked of a value that is not an object, an element of one that is not an array. So a chain of lookups,
`doc["user"]["name"].as_string("?")`, never fails half-way, as a lookup in Go's `map[string]any` gives the zero
value; [contains](contains.md) tells a missing member from a member that is null.

1. The value of the member under `key`.
2. The same for a string literal, `doc["user"]`, without making a string of it.
3. The element at `index`.

There are no writes through `operator[]`: a value never changes, and [set](set.md) makes a new one.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key of the member, compared character by character, case-sensitive |
| `index` | the position of the element, from 0 |

## Return value

A reference to the member's value or the element, valid as long as this value is; a reference to a null that is
never destroyed when there is none.

## Complexity

- (1–2) Linear in the number of members up to 16, constant on average past them.
- (3) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(R"({"user": {"name": "Ala", "tags": ["a", "b"]}})");
    println(doc["user"]["name"].as_string("?"));
    println(doc["user"]["tags"][1].as_string("?"));

    string key = "tags";
    println(doc["user"][key].to_string());
    println(doc["user"]["tags"][5].to_string());
    println(doc["user"]["name"]["first"].to_string());
    println(doc["nobody"]["name"].as_string("?"));
}
```

Output:

```text
Ala
b
["a","b"]
null
null
?
```

## See also

- [contains](contains.md): whether a member is there
- [at_path](at_path.md): a value by a JSON Pointer
- [elements](elements.md), [members](members.md): all of them
- [sgcl::encoding::json](../json.md)
