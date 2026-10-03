[sgcl](../../README.md) › [encoding](../README.md) › [field](../field.md)

# sgcl::encoding::field::quoted

```cpp
field& quoted() noexcept;
```

Marks a number or a boolean field to be written as a JSON string and read from one: `"12"`, `"true"`. Reading,
the value must be a string: a bare number is `type_mismatch`. The option reaches the elements of a container and
the value of an optional (`["1",null]`); a string field is written as it is, not quoted twice. Go's
`json:",string"`, for an API that sends its 64-bit ids as strings, which JavaScript's numbers would round. XML and
CSV pass over it: there every value is text.

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

struct tweet {
    int64_t id = 0;
    bool retweeted = false;
    vector<int64_t> replies;

    void describe(encoding::field_list& f) {
        f.add("id", id).quoted();
        f.add("retweeted", retweeted).quoted();
        f.add("replies", replies).quoted();
    }
};

int main() {
    println(encoding::json::stringify(tweet{1850000000000000001, true, {7}}).value());
    auto t = encoding::json::parse<tweet>(R"({"id": "42", "retweeted": "false", "replies": []})");
    println("{} {}", t->id, t->retweeted);
    auto bare = encoding::json::parse<tweet>(R"({"id": 42})");
    println(bare.error().message());
}
```

Output:

```text
{"id":"1850000000000000001","retweeted":"true","replies":["7"]}
42 false
1:8 /id: expected an integer, found a number
```

## See also

- [names](names.md): an enum as a name
- [sgcl::encoding::field](../field.md)
