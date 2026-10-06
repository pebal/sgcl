[sgcl](../../README.md) › [encoding](../README.md) › [json](README.md)

# sgcl::encoding::json::patch

```cpp
expected<json, error> patch(const json& operations) const noexcept;
```

A new value: the operations of a JSON Patch ([RFC 6902](https://www.rfc-editor.org/rfc/rfc6902)) applied to this
one in their order, all or nothing — the value never changes, and a patch that fails at its third operation gives
the error, not the value after two. Each operation is an object of `"op"`, `"path"` (a JSON Pointer), and `"from"`
or `"value"` as it needs; members it does not know are passed over (§4).

- `add`: a member set (replaced when it is there), an element inserted before the index, `-` appended; the
  container it goes into must be there.
- `remove`, `replace`: the target must be there.
- `move`: remove and add, refused into one of the value's own children; `copy`: add of a copy.
- `test`: the value at the path equal to the one given, by [==](operator_cmp.md) — numbers by value (`1` is
  `1.0`), objects in any order.

## Parameters

| Parameter | Description |
|---|---|
| `operations` | the patch: an array of operations |

## Return value

The new value, or the [error](../error/README.md) of the first operation that fails, its
[path](../error/path.md) that operation's place in the patch (`/2`): `syntax` for a patch that is not an array, an
operation that is not an object or lacks what it needs (`op`, `path`, `from`, `value`), an unknown `op`, a pointer
that is not one, a remove of the whole document, a move into a child; `missing_field` for a path or a from with no
value, an add under a parent that is not there; `out_of_range` for an index past an array's end, a token that is no
index, `-` where no element is; `type_mismatch` for a test whose value differs.

## Complexity

Linear in the size of the value for each operation, which copies the containers on its path.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto doc = encoding::json::parse(R"({"user": {"name": "Ala", "tags": ["a"]}})").value();
    auto ops = encoding::json::parse(R"([
        {"op": "replace", "path": "/user/name", "value": "Ola"},
        {"op": "add", "path": "/user/tags/-", "value": "b"},
        {"op": "copy", "from": "/user/name", "path": "/owner"},
        {"op": "test", "path": "/user/tags/0", "value": "a"}])").value();
    auto changed = doc.patch(ops);
    println(changed ? changed->to_string() : changed.error().message());

    auto bad = encoding::json::parse(R"([{"op": "remove", "path": "/user/age"}])").value();
    println(doc.patch(bad).error().message());
}
```

Output:

```text
{"user":{"name":"Ola","tags":["a","b"]},"owner":"Ola"}
/0: remove: no value at /user/age
```

## See also

- [merge_patch](merge_patch.md): RFC 7396
- [diff](diff.md): a patch made of two values
- [set_path](set_path.md)
- [sgcl::encoding::json](README.md)
