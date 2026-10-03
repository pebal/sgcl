[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::json

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json;
}
```

`sgcl::encoding::json` is one JSON value ([RFC 8259](https://www.rfc-editor.org/rfc/rfc8259)): null, a boolean,
a number, a string, an array or an object. It is immutable, as a [string](../core/string.md) is: a copy is a copy
of the handle, a value is shared between threads with no lock, and a change — [set](json/set.md),
[erase](json/erase.md), [push_back](json/push_back.md), [set_path](json/set_path.md) — returns a new value and
leaves the old one as it was. A value is read with methods, `doc["user"]["name"].as_string()`, and a lookup that
finds nothing gives null, so a chain of lookups never fails half-way. An array or an object made in a loop is made
with a [builder](json-builder.md), without a copy per step.

It is what Go's `any` from `json.Unmarshal` is, and nlohmann's `json` without the writes through `operator[]`;
`std` has no JSON. Where Go's `any` makes every number a `float64`, an integer here keeps its value exactly.

The same class reads and writes the program's own types: [parse](json/parse.md)`<T>`,
[stringify](json/stringify.md), [from](json/from.md) and [as](json/as.md)`<T>` take a type described by its fields
([field_list](field_list.md)) or any kind a field may have, and a file is one call each way,
[load](json/load.md) and [save](json/save.md). A text too large to hold whole, or a stream of values, is read a
piece at a time by a [reader](json-reader.md) and written by a [writer](json-writer.md). What fails in the input is
an [error](error.md) with the place it failed at, never an exception.

## Rules

- **A json is 24 bytes:** a tracked pointer to what does not fit in a word, a word for a boolean or a number, and
  the kind. It holds a `tracked_ptr`, so it lives where one may: on a stack or inside a managed object, in a
  container of the library ([the rules of core](../core/README.md#the-rules), 1). A string is its
  [string](../core/string.md)'s object; an array is its elements side by side in one managed buffer, an object
  its members side by side, so [elements](json/elements.md) and [members](json/members.md) are slices of the
  buffer and walk it in order. A number costs nothing past the 24 bytes. The tree of a text in memory takes about
  the text's size to three times it: numbers in short arrays cost the most (a buffer each), objects of repeated
  keys the least (the keys shared).
- **Numbers keep their value.** An integer literal is an `int64_t` when one holds it, else an `uint64_t`, else it
  is kept as its text — `123456789012345678901234567890` is never rounded, where Go's `any` makes it a
  `float64`. Any other number is a `double`, rounded once from the decimal (`1e400` is `out_of_range`, `1e-400`
  is 0); with [options](json-options.md)`::keep_number_text` it is kept as its literal, for amounts that cannot
  pass through a double, and [number_text](json/number_text.md) gives the digits. `-0` is the double −0.
- **A number is written as JavaScript and Go write it:** the shortest digits that read back as the same double,
  fixed from 1e-6 to 1e21 and with an exponent outside (`1e+21`, `1e-7`), no `.0` on an integer, −0 as `-0`. A
  double of an integer below 1e21 is written in digits without an exponent (`1e20` is `100000000000000000000`)
  and read back as an integer equal to it.
- **[as_int](json/as_int.md) and the others give the value exactly or not at all:** `2.0` is 2, `2.5` and 2^63
  are `nullopt` for `as_int()`; [as_double](json/as_double.md) gives any number, rounded.
- **Equal by value** ([operator==](json/operator_cmp.md)). Two integers compare exactly; an integer and a double
  compare as doubles (`1 == 1.0`, and an integer past 2^53 equals the double it rounds to — what a double is
  written as, read back, equals it); two numbers kept as text compare by their digits. Objects compare as sets of
  members, in any order, as JSON means them; arrays in order. Equal values [hash](json/hash.md) alike.
- **An object keeps the order of its input,** members side by side; past 16 members it has a hash index too, a
  table of `uint32_t` indexes keyed by the hash of `sgcl::string`, which a text from outside cannot aim at. A key
  given twice is an error when parsing (`options::allow_duplicate_keys`: the last one wins);
  [object](json/object.md) and the [builder](json-builder.md) keep the last one.
- **The text is always there:** [to_string](json/to_string.md) cannot fail on the value. A string's invalid UTF-8
  is written as U+FFFD, and a json never holds NaN or an infinity (made from one, it is an assertion in a debug
  build and null in a release one, as `JSON.stringify` writes them).
- **Deep values cost no stack.** Parsing, writing, comparing and hashing walk the tree with a stack of their own:
  `options::max_depth` (512) bounds what a parse takes, and a value built by hand may be deeper.
- **JSON Pointer** ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)) names a value inside another:
  [at_path](json/at_path.md) reads it, [set_path](json/set_path.md) makes the value with it replaced or added.
- **The keys of one parse are made once:** a thousand objects with the same fields share each key's string. The
  parser keeps, per thread and between calls, its stacks and a table of up to 256 keys of at most 32 bytes each,
  so that a key met in one document is shared with the next; a longer key is never kept.
- **A program's types go through their fields:** an error of [parse](json/parse.md)`<T>` or [as](json/as.md)`<T>`
  carries the path of the value that failed (`3:14 /manager/age: expected an integer, found a string`), and
  [stringify](json/stringify.md) fails where a value has no text: NaN, an enum's value past its names, nesting
  past 512 (a cycle of pointers).
- **What waits:** `parse` of a stream reads it on the thread that calls it, `co_await async_parse(in)` in a task;
  the whole stream is one value. `load` and `save` work on the calling thread, their `async_` forms on the
  [blocking pool](../async/spawn_blocking.md).

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `json.Unmarshal(b, &v)` with `v any` | `json::parse(text)`: immutable; integers exact where Go makes them `float64`; the defaults are v2's |
| `json.Unmarshal(b, &s)` into a struct | `json::parse<T>(text)`, `v.as<T>()`: the fields by `describe` ([field_list](field_list.md)); an integer field takes `1.0` and `1e2`, which v2 refuses |
| struct tags, `json.Marshal(s)` of a struct | `describe(field_list&)` and `json::stringify(s)`: one description for every format; the text Go writes, the fields in order, the keys of a map sorted |
| `json.Marshal(v)`, `json.MarshalIndent(v, "", "  ")` | `v.to_string()`, `v.to_string(json::pretty)`: the same text |
| `json.Number`, `UseNumber` | `options::keep_number_text`, `number_text()`; integers are never rounded anyway |
| `map[string]any` lookups, type switches | `doc["a"]`, `as_int()`, `type()`: null for what is not there |
| v2 `AllowDuplicateNames`, `AllowInvalidUTF8` | `options::allow_duplicate_keys`, `allow_invalid_utf8`; the defaults are v2's |
| `SetEscapeHTML` | `style::escape_html`, off by default, as in v2 |
| x/exp `jsonpointer` | `at_path`, `set_path`: RFC 6901 |
| `json.Valid` | `json::reader(text).skip()` and no `more()` after it ([reader](json-reader.md)) |

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](error.md) |
| [kind](json-kind.md) | the kind of a value, an enumeration: `null`, `boolean`, `number`, `string`, `array`, `object` |
| [member](json-member.md) | a member of an object: its key and its value |
| [options](json-options.md) | what a parse and a reader accept |
| [style](json-style.md) | how a value is written |
| [builder](json-builder.md) | an array or an object made in a loop |
| [reader](json-reader.md) | JSON read a piece at a time, from a text or a stream |
| [writer](json-writer.md) | JSON written a piece at a time into a stream |
| [token](json-token.md) | a piece of JSON the reader gives |

## Member objects

| Object | Description |
|---|---|
| `compact` | `static const style`: no space at all, the default of [to_string](json/to_string.md) and [stringify](json/stringify.md) |
| `pretty` | `static const style`: an indent of 2 |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](json/json.md) | null, a boolean, a number or a string |
| `(destructor)` | drops the handle; what it held is left to the collector |

#### Making a value

| Function | Description |
|---|---|
| [array](json/array.md) | an array of values (static) |
| [object](json/object.md) | an object of members (static) |
| [from](json/from.md) | the value of a program's type (static) |

#### Reading and writing

| Function | Description |
|---|---|
| [parse, async_parse](json/parse.md) | the value of a text or a stream, or a program's type of it (static) |
| [to_string](json/to_string.md) | the text of the value |
| [stringify](json/stringify.md) | the text of a program's value (static) |
| [as](json/as.md) | the value as a program's type |
| [load, async_load](json/load.md) | the value of a file, or a program's type of it (static) |
| [save, async_save](json/save.md) | a program's value, or this value, into a file |

#### Kind

| Function | Description |
|---|---|
| [type](json/type.md) | the kind of the value |
| [is_null](json/is_null.md) | checks whether the value is null |
| [is_bool](json/is_bool.md) | checks whether the value is a boolean |
| [is_number](json/is_number.md) | checks whether the value is a number |
| [is_integer](json/is_integer.md) | checks whether the value is a number an `int64_t` or an `uint64_t` holds exactly |
| [is_string](json/is_string.md) | checks whether the value is a string |
| [is_array](json/is_array.md) | checks whether the value is an array |
| [is_object](json/is_object.md) | checks whether the value is an object |

#### Value

| Function | Description |
|---|---|
| [as_bool](json/as_bool.md) | the boolean |
| [as_int](json/as_int.md) | the number as an `int64_t`, exactly |
| [as_uint](json/as_uint.md) | the number as an `uint64_t`, exactly |
| [as_double](json/as_double.md) | the number as a `double`, rounded |
| [as_string](json/as_string.md) | the string |
| [number_text](json/number_text.md) | the literal of a number kept as text |

#### Element access

| Function | Description |
|---|---|
| [operator[]](json/operator_at.md) | a member by its key, an element by its index; null when there is none |
| [at_path](json/at_path.md) | the value at a JSON Pointer |
| [elements](json/elements.md) | the elements of an array, as a slice |
| [members](json/members.md) | the members of an object, as a slice |

#### Lookup

| Function | Description |
|---|---|
| [contains](json/contains.md) | checks whether an object has a member under a key |

#### Capacity

| Function | Description |
|---|---|
| [empty](json/empty.md) | checks whether the value has no elements or members |
| [size](json/size.md) | the number of elements or members |

#### New versions

| Function | Description |
|---|---|
| [set](json/set.md) | the value with a member or an element set |
| [erase](json/erase.md) | the object without a member |
| [push_back](json/push_back.md) | the array with an element appended |
| [set_path](json/set_path.md) | the value with the one at a JSON Pointer replaced or added |

#### Hashing

| Function | Description |
|---|---|
| [hash](json/hash.md) | the hash of the value, alike for equal values |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](json/operator_cmp.md) | compares two values by value |

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

- [field_list](field_list.md): the program's types, described by their fields
- [reader](json-reader.md), [writer](json-writer.md): JSON a piece at a time
- [error](error.md): why a text is not JSON, and where
- [string](../core/string.md): the other immutable value of the library
- [sgcl::encoding](README.md)
