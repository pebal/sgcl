[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::merge_patch

```cpp
json merge_patch(const json& patch) const noexcept;
```

A new value: the JSON Merge Patch of [RFC 7396](https://www.rfc-editor.org/rfc/rfc7396) applied to this one. A
patch that is an object is merged member by member: a member of null removes the member, a member that is an object
merges into the member's value (an object made of what is not one), any other value replaces it; a patch that is not
an object is the value instead of this. The members keep their places, the new ones after them. It never fails, and
cannot set a member to null or change an array's element alone: [patch](patch.md) does what it cannot.

## Parameters

| Parameter | Description |
|---|---|
| `patch` | the merge patch |

## Return value

The new value.

## Complexity

Linear in the sizes of the value and the patch.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto doc = encoding::json::parse(
        R"({"title": "Goodbye!", "author": {"givenName": "John", "familyName": "Doe"}, "tags": ["example"]})").value();
    auto patch = encoding::json::parse(
        R"({"title": "Hello!", "phoneNumber": "+01-123-456-7890", "author": {"familyName": null}})").value();
    println(doc.merge_patch(patch).to_string());
}
```

Output:

```text
{"title":"Hello!","author":{"givenName":"John"},"tags":["example"],"phoneNumber":"+01-123-456-7890"}
```

## See also

- [patch](patch.md): RFC 6902
- [sgcl::encoding::json](README.md)
