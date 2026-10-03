[sgcl](../README.md) › [encoding](README.md)

# sgcl::encoding::field_list

```cpp
#include "sgcl/encoding/fields.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class field_list;
}
```

`sgcl::encoding::field_list` is the description of a type of the program by its fields, written once for every
format of the module: a method `void describe(field_list& f)` that names each field, `f.add("name", name)`, and
sets its options on the [field](field.md) that `add` returns, `f.add("age", age).omit_empty()`. With it, the type
is read and written by JSON ([json::parse\<T\>](json/parse.md), [json::stringify](json/stringify.md),
[json::as\<T\>](json/as.md), [json::reader::read\<T\>](json-reader/read.md),
[json::writer::value](json-writer/value.md)), by XML ([xml::parse\<T\>](xml/parse.md),
[xml::stringify](xml/stringify.md), [xml::as\<T\>](xml/as.md), [xml::from](xml/from.md),
[xml::reader](xml-reader.md)`::read<T>`, [xml::writer](xml-writer.md)`::value`: a field is a child element,
[attribute()](field/attribute.md) an attribute, [text()](field/text.md) the element's text), by CSV
([csv::reader::read\<T\>](csv-reader/read.md), [csv::writer::write](csv-writer/write.md): a field is a column, by
the header's name), and by [slog](../slog/README.md), which logs it as a group of its fields.

It is what the tags of a Go structure are (`json:"name,omitempty"`), written as code: no macro, no pointer to a
member, no template on the side of the type, and the type stays an aggregate. `std` has no reflection of a
type's members to do it with. The list is filled by `describe` and read by a format; a program writes `describe`
and calls [add](field_list/add.md), and never reads a list itself.

## Rules

- **One method, both ways, every format.** `describe` names the fields; reading fills them, writing reads them.
  A field may be private: the method sees it. A base class is described by calling its `describe` first,
  `base::describe(f);`.
- **A type of another library** is described by a free function `void describe(field_list& f, lib::point& p)` in
  its namespace or in `sgcl`, which argument-dependent lookup finds.
- **Two doors for a type that fields cannot describe.** `json to_json() const` with
  `static optional<T> from_json(const json&)` gives a JSON of its own (`[x, y]` for a point), and JSON alone has
  it; `string to_text() const` with `static optional<T> from_text(const string&)` gives one text for every
  format — a JSON string, a CSV field, an XML attribute or element, a key of a map (a uuid, a date). `from_json`
  or `from_text` returning `nullopt` is `type_mismatch` at the place of the value. A type with either door is
  read and written through it, its `describe` aside.
- **A hash map and a hash set are written sorted**, by the text of the key or of the element, as Go sorts the
  keys of a map: the text of a value does not depend on the hash of its keys. [json::style](json-style.md)
  `::sort_keys = false` keeps their own order. A sorted or an ordered container is written in its order.
- **Reading an object**: the fields by their names, case-sensitive; a key no field has is skipped
  ([json::options](json-options.md)`::reject_unknown_fields`: `unknown_field`); a key given twice is
  `duplicate_key`; a field that is not there keeps its value, unless it is [required()](field/required.md):
  `missing_field`. JSON's null in a field that is not an optional or a pointer puts the default value there, as
  Go's v2 does.
- **An integer field takes a number whose value is an integer**: `1.0` and `1e2` are 1 and 100, which Go's v2
  refuses; `1.5` is `type_mismatch`, `300` into an `uint8_t` and `-1` into an unsigned field `out_of_range`. A
  `float` field is rounded once from the decimal, never a `double` rounded again, and written with a float's
  shortest digits.
- **The errors say where**: the path of the value that failed as a JSON Pointer (`/users/3/age`), and in a text
  the line and the column ([error](error.md)). Writing, a cycle of pointers is found by the depth (512,
  `unsupported_value`), and NaN is `unsupported_value` with its path.
- **Nothing is kept between two objects.** `describe` is called on the object each time it is read or written,
  and the list it fills is the format's, on the stack of the call (a CSV reader keeps one and empties it for each
  record); the operations of each type are one constant table, which holds no tracked pointer. A described type needs a default constructor, which the compiler checks: an element
  of a container, the object behind a pointer and a record of `read<T>` are made new and then filled.
- **A container of `std` holds only what holds no tracked pointer** — numbers, enums, `std::string`, `std`'s
  optionals, pairs, tuples, arrays and vectors of them — which the compiler checks: its memory is not the
  collector's. The library's containers hold anything.

### The types a field may have

| Type | In JSON |
|---|---|
| `bool` | `true`, `false` |
| an integer type, but the characters (a `char` field is a compile-time error) | a number, range-checked when read |
| `float`, `double` | a number, with the type's own shortest digits |
| [string](../core/string.md), `std::string` | a string |
| [json](json.md) | the value as it is, Go's `RawMessage` |
| `optional<T>` | null or the value |
| `tracked_ptr<T>` | null, or a new object when read |
| `vector`, `deque`, `list`, `dynamic_array`, `immutable::vector`, `immutable::list`, any range with `push_back`; `std::vector`, `std::deque`, `std::list` | an array |
| `array<T, N>`, `std::array<T, N>`, `T[N]` | an array of exactly N |
| `set`, `sorted_set`, `ordered_set`, `immutable::set` | an array |
| `map`, `sorted_map`, `ordered_map`, `immutable::map`, keyed by a string, an integer or a type with `to_text` | an object, the key's text its name |
| `pair`, `tuple` | an array of their elements |
| `variant` with [tagged](field/tagged.md) | an object with a tag; without `tagged`, `unsupported_value` |
| an enum | its integer, or one of its [names](field/names.md) |
| a type with `describe` | an object of its fields |
| a type with `to_json` and `from_json` | its own JSON |
| a type with `to_text` and `from_text` | a string |

A type that is none of these is a compile-time error.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| a struct with tags, `json:"name"` | `void describe(field_list& f) { f.add("name", name); }`: one method for every format and both ways |
| `json.Marshal` and `json.Unmarshal` of a struct | `json::stringify`, `json::parse<T>`, the fields by `describe` |
| `json:",omitempty"`, `json:",string"`, `xml:",attr"`, `xml:",chardata"`; v2 has no required | the options of a [field](field.md): `omit_empty()`, `quoted()`, `attribute()`, `text()`, `required()` |
| `Marshaler`, `Unmarshaler` | `to_json()`, `from_json(const json&)` |
| `TextMarshaler`, `TextUnmarshaler` | `to_text()`, `from_text(const string&)`: a JSON string, a CSV field, an XML attribute, a map's key |
| interfaces and `any` in a struct | a `variant` with `.tagged("type", {...})`; a field of type `json` |
| `UnmarshalTypeError.Field` | `error.path()`: a JSON Pointer, and the line and the column |
| `encoding/gob`, `binary.Read` and `binary.Write` of a struct | none yet: a serialization of object graphs, after JSON, will read the same `describe` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](field_list/field_list.md) | constructs an empty list |
| `(destructor)` | destroys the list |

#### Modifiers

| Function | Description |
|---|---|
| [add](field_list/add.md) | adds a field: its name and the member |

#### Capacity

| Function | Description |
|---|---|
| [size](field_list/size.md) | the number of fields |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

enum class role { reader, writer, admin };

struct address {
    string city;
    string street;

    void describe(encoding::field_list& f) {
        f.add("city", city);
        f.add("street", street);
    }
};

struct user {
    string name;
    int age = 0;
    vector<string> tags;
    optional<address> home;
    tracked_ptr<user> manager;
    role access = role::reader;
    map<string, int> scores;

    void describe(encoding::field_list& f) {
        f.add("name", name).required();
        f.add("age", age).omit_empty();
        f.add("tags", tags);
        f.add("home", home);
        f.add("manager", manager);
        f.add("access", access).names({"reader", "writer", "admin"});
        f.add("scores", scores).omit_empty();
    }
};

int main() {
    auto u = encoding::json::parse<user>(R"({"name": "Ala", "age": 30, "tags": ["a"], "access": "admin",
        "manager": {"name": "Ola", "home": {"city": "Kraków", "street": "Długa"}}})");
    println("{} reports to {} in {}", u->name, u->manager->name, u->manager->home->city);

    u->scores = {{"go", 3}, {"cpp", 5}};
    println(encoding::json::stringify(*u).value());

    auto wrong = encoding::json::parse<user>(R"({"name": "Ala", "manager": {"name": "Ola", "age": "old"}})");
    println(wrong.error().message());
    auto missing = encoding::json::parse<user>(R"({"age": 3})");
    println(missing.error().message());
}
```

Output:

```text
Ala reports to Ola in Kraków
{"name":"Ala","age":30,"tags":["a"],"home":null,"manager":{"name":"Ola","tags":[],"home":{"city":"Kraków","street":"Długa"},"manager":null,"access":"reader"},"access":"admin","scores":{"cpp":5,"go":3}}
1:51 /manager/age: expected an integer, found a string
1:10 /name: missing field
```

## See also

- [field](field.md): the options of a field
- [json](json.md): [parse\<T\>](json/parse.md), [stringify](json/stringify.md), [as\<T\>](json/as.md)
- [json::reader](json-reader.md), [json::writer](json-writer.md): a record at a time
- [xml](xml.md), [csv::reader](csv-reader.md), [csv::writer](csv-writer.md): the other formats
- [error](error.md): the path of what failed
- [sgcl::encoding](README.md)
