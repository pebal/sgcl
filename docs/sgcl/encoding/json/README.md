[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::json

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::json` is one JSON value ([RFC 8259](https://www.rfc-editor.org/rfc/rfc8259)): null, a boolean,
a number, a string, an array or an object. It is immutable, as a [string](../../core/string/README.md) is: a copy is a copy
of the handle, a value is shared between threads with no lock, and a change — [set](set.md),
[erase](erase.md), [push_back](push_back.md), [set_path](set_path.md) — returns a new value and
leaves the old one as it was. A value is read with methods, `doc["user"]["name"].as_string()`, and a lookup that
finds nothing gives null, so a chain of lookups never fails half-way. An array or an object made in a loop is made
with a [builder](../json-builder/README.md), without a copy per step.

It is what Go's `any` from `json.Unmarshal` is, and nlohmann's `json` without the writes through `operator[]`;
`std` has no JSON. Where Go's `any` makes every number a `float64`, an integer here keeps its value exactly.

The same class reads and writes the program's own types: [parse](parse.md)`<T>`,
[stringify](stringify.md), [from](from.md) and [as](as.md)`<T>` take a type described by its fields
([field_list](../field_list/README.md)) or any kind a field may have, and a file is one call each way,
[load](load.md) and [save](save.md). A text too large to hold whole, or a stream of values, is read a
piece at a time by a [reader](../json-reader/README.md) and written by a [writer](../json-writer/README.md). What fails in the input is
an [error](../error/README.md) with the place it failed at, never an exception.

## Rules

- **A json is 24 bytes:** a tracked pointer to what does not fit in a word, a word for a boolean or a number, and the
  kind. A string is its [string](../../core/string/README.md)'s object; an array is its elements side by side in one
  managed buffer, an object its members side by side, so [elements](elements.md) and [members](members.md) are slices of
  the buffer and walk it in order. A number costs nothing past the 24 bytes. The tree of a text in memory takes about
  the text's size to three times it: numbers in short arrays cost the most (a buffer each), objects of repeated keys the
  least (the keys shared).
- **Numbers keep their value.** An integer literal is an `int64_t` when one holds it, else an `uint64_t`, else it
  is kept as its text — `123456789012345678901234567890` is never rounded, where Go's `any` makes it a
  `float64`. Any other number is a `double`, rounded once from the decimal (`1e400` is `out_of_range`, `1e-400`
  is 0); with [options](../json-options.md)`::keep_number_text` it is kept as its literal, for amounts that cannot
  pass through a double, and [number_text](number_text.md) gives the digits. `-0` is the double −0.
- **A number is written as JavaScript and Go write it:** the shortest digits that read back as the same double,
  fixed from 1e-6 to 1e21 and with an exponent outside (`1e+21`, `1e-7`), no `.0` on an integer, −0 as `-0`. A
  double of an integer below 1e21 is written in digits without an exponent (`1e20` is `100000000000000000000`)
  and read back as an integer equal to it.
- **[as_int](as_int.md) and the others give the value exactly or not at all:** `2.0` is 2, `2.5` and 2^63
  are `nullopt` for `as_int()`; [as_double](as_double.md) gives any number, rounded.
- **Equal by value** ([operator==](operator_cmp.md)). Two integers compare exactly; an integer and a double
  compare as doubles (`1 == 1.0`, and an integer past 2^53 equals the double it rounds to — what a double is
  written as, read back, equals it); two numbers kept as text compare by their digits. Objects compare as sets of
  members, in any order, as JSON means them; arrays in order. Equal values [hash](hash.md) alike.
- **An object keeps the order of its input,** members side by side; past 16 members it has a hash index too, a
  table of `uint32_t` indexes keyed by the hash of `sgcl::string`, which a text from outside cannot aim at. A key
  given twice is an error when parsing (`options::allow_duplicate_keys`: the last one wins);
  [object](object.md) and the [builder](../json-builder/README.md) keep the last one.
- **The text is always there:** [to_string](to_string.md) cannot fail on the value. A string's invalid UTF-8
  is written as U+FFFD, and a json never holds NaN or an infinity (made from one, it is an assertion in a debug
  build and null in a release one, as `JSON.stringify` writes them).
- **Deep values cost no stack.** Parsing, writing, comparing and hashing walk the tree with a stack of their own:
  `options::max_depth` (512) bounds what a parse takes, and a value built by hand may be deeper.
- **JSON Pointer** ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)) names a value inside another:
  [at_path](at_path.md) reads it, [set_path](set_path.md) makes the value with it replaced or added,
  [erase_path](erase_path.md) without it, [path_of](path_of.md) makes a pointer of keys.
- **JSON Patch** ([RFC 6902](https://www.rfc-editor.org/rfc/rfc6902)) and **JSON Merge Patch**
  ([RFC 7396](https://www.rfc-editor.org/rfc/rfc7396)): [patch](patch.md) applies a list of operations all or
  nothing, [merge_patch](merge_patch.md) merges an object into the value, [diff](diff.md) makes the patch from one
  value to another.
- **The keys of one parse are made once:** a thousand objects with the same fields share each key's string. The
  parser keeps, per thread and between calls, its stacks and a table of up to 256 keys of at most 32 bytes each,
  so that a key met in one document is shared with the next; a longer key is never kept.
- **A program's types go through their fields:** an error of [parse](parse.md)`<T>` or [as](as.md)`<T>`
  carries the path of the value that failed (`3:14 /manager/age: expected an integer, found a string`), and
  [stringify](stringify.md) fails where a value has no text: NaN, an enum's value past its names, nesting
  past 512 (a cycle of pointers).
- **What waits:** `parse` of a stream reads it on the thread that calls it, `co_await async_parse(in)` in a task;
  the whole stream is one value. `load` and `save` work on the calling thread, their `async_` forms on the
  [blocking pool](../../async/spawn_blocking.md).

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `json.Unmarshal(b, &v)` with `v any` | `json::parse(text)`: immutable; integers exact where Go makes them `float64`; the defaults are v2's |
| `json.Unmarshal(b, &s)` into a struct | `json::parse<T>(text)`, `v.as<T>()`: the fields by `describe` ([field_list](../field_list/README.md)); an integer field takes `1.0` and `1e2`, which v2 refuses |
| struct tags, `json.Marshal(s)` of a struct | `describe(field_list&)` and `json::stringify(s)`: one description for every format; the text Go writes, the fields in order, the keys of a map sorted |
| `json.Marshal(v)`, `json.MarshalIndent(v, "", "  ")` | `v.to_string()`, `v.to_string(json::pretty)`: the same text |
| `json.Number`, `UseNumber` | `options::keep_number_text`, `number_text()`; integers are never rounded anyway |
| `map[string]any` lookups, type switches | `doc["a"]`, `as_int()`, `type()`: null for what is not there |
| v2 `AllowDuplicateNames`, `AllowInvalidUTF8` | `options::allow_duplicate_keys`, `allow_invalid_utf8`; the defaults are v2's |
| `SetEscapeHTML` | `style::escape_html`, off by default, as in v2 |
| x/exp `jsonpointer` | `at_path`, `set_path`, `erase_path`, `path_of`: RFC 6901 |
| `evanphx/json-patch`'s `Apply`, `MergePatch` | `patch` (RFC 6902), `merge_patch` (RFC 7396), `diff` |
| `json.Valid` | `json::reader(text).skip()` and no `more()` after it ([reader](../json-reader/README.md)) |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| [kind](../json-kind.md) | the kind of a value, an enumeration: `null`, `boolean`, `number`, `string`, `array`, `object` |
| [member](../json-member.md) | a member of an object: its key and its value |
| [options](../json-options.md) | what a parse and a reader accept |
| [style](../json-style.md) | how a value is written |
| [builder](../json-builder/README.md) | an array or an object made in a loop |
| [reader](../json-reader/README.md) | JSON read a piece at a time, from a text or a stream |
| [writer](../json-writer/README.md) | JSON written a piece at a time into a stream |
| [token](../json-token/README.md) | a piece of JSON the reader gives |

## Member objects

| Object | Description |
|---|---|
| `compact` | `static const style`: no space at all, the default of [to_string](to_string.md) and [stringify](stringify.md) |
| `pretty` | `static const style`: an indent of 2 |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json.md) | null, a boolean, a number or a string |
| `(destructor)` | drops the handle; what it held is left to the collector |

#### Making a value

| Function | Description |
|---|---|
| [array](array.md) | an array of values (static) |
| [object](object.md) | an object of members (static) |
| [from](from.md) | the value of a program's type (static) |
| [diff](diff.md) | the JSON Patch from one value to another (static) |

#### Reading and writing

| Function | Description |
|---|---|
| [parse, async_parse](parse.md) | the value of a text or a stream, or a program's type of it (static) |
| [to_string](to_string.md) | the text of the value |
| [stringify](stringify.md) | the text of a program's value (static) |
| [as](as.md) | the value as a program's type |
| [load, async_load](load.md) | the value of a file, or a program's type of it (static) |
| [save, async_save](save.md) | a program's value, or this value, into a file |

#### Kind

| Function | Description |
|---|---|
| [type](type.md) | the kind of the value |
| [is_null](is_null.md) | checks whether the value is null |
| [is_bool](is_bool.md) | checks whether the value is a boolean |
| [is_number](is_number.md) | checks whether the value is a number |
| [is_integer](is_integer.md) | checks whether the value is a number an `int64_t` or an `uint64_t` holds exactly |
| [is_string](is_string.md) | checks whether the value is a string |
| [is_array](is_array.md) | checks whether the value is an array |
| [is_object](is_object.md) | checks whether the value is an object |

#### Value

| Function | Description |
|---|---|
| [as_bool](as_bool.md) | the boolean |
| [as_int](as_int.md) | the number as an `int64_t`, exactly |
| [as_uint](as_uint.md) | the number as an `uint64_t`, exactly |
| [as_double](as_double.md) | the number as a `double`, rounded |
| [as_string](as_string.md) | the string |
| [number_text](number_text.md) | the literal of a number kept as text |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | a member by its key, an element by its index; null when there is none |
| [at_path](at_path.md) | the value at a JSON Pointer |
| [path_of](path_of.md) | the JSON Pointer of keys, escaped (static) |
| [elements](elements.md) | the elements of an array, as a slice |
| [members](members.md) | the members of an object, as a slice |

#### Lookup

| Function | Description |
|---|---|
| [contains](contains.md) | checks whether an object has a member under a key |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the value has no elements or members |
| [size](size.md) | the number of elements or members |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | the value with a member or an element set |
| [erase](erase.md) | the object without a member |
| [push_back](push_back.md) | the array with an element appended |
| [set_path](set_path.md) | the value with the one at a JSON Pointer replaced or added |
| [erase_path](erase_path.md) | the value without the one at a JSON Pointer |
| [patch](patch.md) | the value with a JSON Patch applied, all or nothing |
| [merge_patch](merge_patch.md) | the value with a JSON Merge Patch applied |

#### Hashing

| Function | Description |
|---|---|
| [hash](hash.md) | the hash of the value, alike for equal values |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | compares two values by value |

## Complexity

- A lookup by key: linear in the number of members up to 16, constant on average past them (the hash index).
- A lookup by index, the kind and the value of a scalar: constant.
- A new version: linear in the size of the array or the object it changes, which it copies; `set_path` that for
  every container on the path. The elements themselves are handles, copied without their contents.
- Parsing, writing, comparing and hashing: linear in the size of the text or of the tree.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(
        R"({"user": {"name": "Ala", "tags": ["a","b"]}, "count": 3, "id": 123456789012345678901})");

    string name = doc["user"]["name"].as_string("?");
    int64_t count = doc["count"].as_int(0);
    println("{} {}", name, count);
    for (auto& tag : doc["user"]["tags"].elements()) {
        println(tag.as_string(""));
    }
    for (auto& [key, value] : doc.members()) {
        println("{} is {}", key, value.to_string());
    }
    println(doc["missing"]["deeper"].to_string());

    // a new version; doc is as it was
    auto renamed = doc.set_path("/user/name", "Ola").set_path("/user/tags/-", "c");
    println(renamed["user"].to_string(encoding::json::pretty));

    encoding::json::builder squares;
    for (int i : range(5)) {
        squares.push_back(i * i);
    }
    auto report = encoding::json::object(
        {{"count", 5}, {"squares", squares.build()}, {"ratio", 0.1}});
    println(report.to_string());

    auto bad = encoding::json::parse("{\"a\": [1, 2,]}");
    println(bad.error().message());
}
```

Output:

```text
Ala 3
a
b
user is {"name":"Ala","tags":["a","b"]}
count is 3
id is 123456789012345678901
null
{
  "name": "Ola",
  "tags": [
    "a",
    "b",
    "c"
  ]
}
{"count":5,"squares":[0,1,4,9,16],"ratio":0.1}
1:13: invalid character ']' where a value was expected
```

## See also

- [field_list](../field_list/README.md): the program's types, described by their fields
- [reader](../json-reader/README.md), [writer](../json-writer/README.md): JSON a piece at a time
- [error](../error/README.md): why a text is not JSON, and where
- [string](../../core/string/README.md): the other immutable value of the library
- [sgcl::encoding](../README.md)
