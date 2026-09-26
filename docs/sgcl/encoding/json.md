# sgcl::encoding::json

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class json;            // one JSON value: null, a boolean, a number, a string, an array or an object
    struct json::member;   // a member of an object: key and value
    class json::builder;   // an array or an object made in a loop
    struct json::options;  // what parse and the reader accept
    struct json::style;    // how a value is written
}
```

One JSON value ([RFC 8259](https://www.rfc-editor.org/rfc/rfc8259)): null, a boolean, a number, a string, an array or an object. It is **immutable**, as a [`string`](../core/string.md) is: a copy is a copy of the handle, a value is shared between threads with no lock, and a change — `set`, `erase`, `push_back`, `set_path` — returns a new value and leaves the old one as it was. It is read with methods, `doc["user"]["name"].as_string()`, and a lookup that finds nothing gives null, so a chain of lookups never fails half-way. What Go's `any` from `json.Unmarshal` is, and nlohmann's `json` without the writes through `operator[]`.

## Rules

- **Numbers keep their value.** An integer literal is an `int64` when one holds it, else an `uint64`, else it is kept as its text — `123456789012345678901234567890` is never rounded, where Go's `any` makes it a float64. Any other number is a double, rounded once from the decimal (`1e400` is `out_of_range`, `1e-400` is 0); with `options::keep_number_text` it is kept as its literal, for amounts that cannot pass through a double, and `number_text()` gives the digits. `-0` is the double −0.
- **A number is written as JavaScript and Go write it**: the shortest digits that read back as the same double, fixed from 1e-6 to 1e21 and with an exponent outside (`1e+21`, `1e-7`), no `.0` on an integer, −0 as `-0`. A double past 2^53 that is an integer is written as the integer (`1e20` is `100000000000000000000`) and read back as one.
- **`as_int()` and the others give the value exactly or not at all**: `2.0` is 2, `2.5` and 2^63 are `nullopt` for `as_int()`; `as_double()` gives any number, rounded.
- **Equal by value.** Two integers compare exactly; an integer and a double compare as doubles (`1 == 1.0`, and an integer past 2^53 equals the double it rounds to — what a double is written as, read back, equals it); two numbers kept as text compare by their digits. Objects compare as sets of members, in any order, as JSON means them; arrays in order. Equal values hash alike.
- **An object keeps the order of its input**, members side by side; past 16 members it has a hash index too, keyed (the hash of `sgcl::string`, which a text from outside cannot aim at). A key given twice is an error when parsing (`options::allow_duplicate_keys`: the last one wins); `object()` and the `builder` keep the last one.
- **The text is always there**: `to_string()` cannot fail. A string's invalid UTF-8 is written as U+FFFD, and a json never holds NaN or an infinity (made from one, it is an assertion in a debug build and null in a release one, as `JSON.stringify` writes them).
- **Deep values cost no stack.** Parsing, writing, comparing and hashing walk the tree with a stack of their own: `max_depth` (512) bounds what a parse takes, and a value built by hand may be deeper.
- **JSON Pointer** ([RFC 6901](https://www.rfc-editor.org/rfc/rfc6901)): `at_path("/users/0/name")`, with `~1` for `/` and `~0` for `~` in a key. `set_path` replaces a member or an element, adds a member, appends with `-` or with the index of the end, and makes the missing objects on the way (null on the way becomes an object); a pointer through a number, a string or a boolean, past the end of an array, or not a pointer at all gives the value unchanged.
- **The keys of one parse are made once**: a thousand objects with the same fields share each key's string.

## Members

```cpp
class json {
public:
    using error = encoding::error;
    enum class kind : uint8_t { null, boolean, number, string, array, object };
    struct member { string key; json value; };
    struct options {
        uint32_t max_depth = 512;
        bool allow_duplicate_keys = false;
        bool allow_invalid_utf8 = false;
        bool keep_number_text = false;
        bool reject_unknown_fields = false;   // parse<T>: a key no field has is an error
        size_t max_token_size = 64 << 20;     // what a reader of a stream holds at once, a token or a value read whole: errc::out_of_range past it
    };
    struct style {
        uint8_t indent = 0;                   // 0: compact
        bool escape_html = false;             // <, >, & as <, >, &
        bool sort_keys = true;                // the keys of a hash map of a typed value
    };
    class builder;
    class reader;                             // json_reader.md
    class writer;                             // json_writer.md
    class token;
    static const style compact;
    static const style pretty;                // an indent of 2

    json() noexcept;                          // null
    json(std::nullptr_t) noexcept;
    json(bool b) noexcept;
    json(I v) noexcept;                       // any integer type but bool and the character types
    json(double d) noexcept;
    json(float f) noexcept;
    json(const string& s) noexcept;
    json(const char* s);
    json(const slice<const char>& s);
    static json array(std::initializer_list<json> elements);
    static json array(const R& elements);     // any range of values a json is made of
    static json object(std::initializer_list<member> members);

    static expected<json, error> parse(const string& text);
    static expected<json, error> parse(const string& text, const options& o);
    static expected<json, error> parse(const io::reader& in);
    static expected<json, error> parse(const io::reader& in, const options& o);
    static task<expected<json, error>> async_parse(const io::reader& in);
    static task<expected<json, error>> async_parse(io::reader in, options o);
    string to_string(const style& s = compact) const;

    // typed values: a type described by its fields (fields.md) or any a field may have
    template<class T> static expected<T, error> parse(const string& text);
    template<class T> static expected<T, error> parse(const string& text, const options& o);
    template<class T> static expected<T, error> parse(const io::reader& in);
    template<class T> static expected<T, error> parse(const io::reader& in, const options& o);
    template<class T> static task<expected<T, error>> async_parse(const io::reader& in);
    template<class T> static task<expected<T, error>> async_parse(io::reader in, options o);
    template<class T> static expected<string, error> stringify(const T& value);
    template<class T> static expected<string, error> stringify(const T& value, const style& s);
    template<class T> static expected<json, error> from(const T& value);   // the value of a T, as xml::from (through its text)
    template<class T> expected<T, error> as() const;
    template<class T> expected<T, error> as(const options& o) const;

    kind type() const noexcept;
    bool is_null() const noexcept;
    bool is_bool() const noexcept;
    bool is_number() const noexcept;
    bool is_integer() const noexcept;         // a number an int64 or an uint64 holds exactly
    bool is_string() const noexcept;
    bool is_array() const noexcept;
    bool is_object() const noexcept;

    optional<bool> as_bool() const noexcept;
    optional<int64_t> as_int() const noexcept;
    optional<uint64_t> as_uint() const noexcept;
    optional<double> as_double() const noexcept;
    optional<string> as_string() const noexcept;
    optional<string> number_text() const;     // the literal of a number kept as text

    const json& operator[](const string& key) const noexcept;    // null when there is none
    const json& operator[](const char (&key)[N]) const noexcept;
    const json& operator[](size_t index) const noexcept;         // null past the end
    bool contains(const string& key) const noexcept;
    size_t size() const noexcept;             // the elements or the members; 0 for the rest
    bool empty() const noexcept;
    slice<const json> elements() const noexcept;
    slice<const member> members() const noexcept;               // in the order of the input
    optional<json> at_path(const string& pointer) const;

    json set(const string& key, const json& value) const;
    json erase(const string& key) const;
    json set(size_t index, const json& value) const;
    json push_back(const json& value) const;
    json set_path(const string& pointer, const json& value) const;

    friend bool operator==(const json& a, const json& b);
    size_t hash() const;
};

class json::builder {
public:
    builder& push_back(const json& value);           // the elements of an array
    builder& set(const string& key, const json& v);  // the members of an object: a key twice keeps the last
    size_t size() const noexcept;
    json build();                                     // the value; the builder empty again ([] when nothing was added)
};
```

`parse<T>`, `stringify`, `from` and `as<T>` read and write a type of the program by its fields — [`field_list`](fields.md) says how: `json::parse<user>(text)` is `expected<user, error>`, and an error carries the path of the value that failed (`3:14 /manager/age: expected an integer, found a string`). `stringify` fails where a value has no text: NaN, an enum's value past its names, nesting past 512 (a cycle of pointers). `parse` of a stream reads it on the thread that calls it, `co_await json::async_parse(in)` in a task; the whole stream is one value. Mixing `push_back` and `set` in one builder is `logic_error`.

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;

int main() {
    auto doc = encoding::json::parse(R"({"user": {"name": "Ala", "tags": ["a", "b"]}, "count": 3, "id": 123456789012345678901})").value();

    string name = doc["user"]["name"].as_string().value_or("?");
    int64_t count = doc["count"].as_int().value_or(0);
    io::stdout.write(name + " " + to_string(count) + "\n");
    for (auto& tag : doc["user"]["tags"].elements()) {
        io::stdout.write(tag.as_string().value_or("") + "\n");
    }
    for (auto& [key, value] : doc.members()) {
        io::stdout.write(key + " is " + value.to_string() + "\n");
    }
    io::stdout.write(doc["missing"]["deeper"].to_string() + "\n");

    // a new version; doc is as it was
    auto renamed = doc.set_path("/user/name", "Ola").set_path("/user/tags/-", "c");
    io::stdout.write(renamed["user"].to_string(encoding::json::pretty) + "\n");

    encoding::json::builder squares;
    for (auto i : range(5)) {
        squares.push_back(i * i);
    }
    auto report = encoding::json::object({{"count", 5}, {"squares", squares.build()}, {"ratio", 0.1}});
    io::stdout.write(report.to_string() + "\n");

    auto bad = encoding::json::parse("{\"a\": [1, 2,]}");
    io::stdout.write(bad.error().message() + "\n");
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

## Memory

24 bytes: a tracked pointer to what does not fit in a word, a word for a boolean or a number, and the kind. A string is its [`string`](../core/string.md)'s object; an array is its elements side by side in one managed buffer, an object its members side by side (with a table of `uint32_t` indexes past 16 members), so `elements()` and `members()` are slices of the buffer and walk it in order. A number costs nothing past the 24 bytes. The tree of a text in memory takes about the text's size to three times it: numbers in short arrays cost the most (a buffer each), objects of repeated keys the least (the keys shared).

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `json.Unmarshal(b, &v)` with `v any` | `json::parse(text)` | immutable; integers exact (Go: float64) |
| `json.Unmarshal(b, &s)` into a struct | `json::parse<T>(text)`, `v.as<T>()` | the fields by `describe` ([fields](fields.md)); an integer field takes `1.0` and `1e2` (v2 refuses them) |
| `json.Marshal(s)` of a struct | `json::stringify(s)` | the text Go writes: the fields in order, map keys sorted |
| `json.Marshal(v)`, `MarshalIndent(v, "", "  ")` | `v.to_string()`, `v.to_string(json::pretty)` | the same text |
| `json.Number`, `UseNumber` | `options::keep_number_text`, `number_text()` | integers are never rounded anyway |
| `map[string]any` lookups, type switches | `doc["a"]`, `as_int()`, `type()` | null for what is not there |
| v2 `AllowDuplicateNames`, `AllowInvalidUTF8` | `options::allow_duplicate_keys`, `allow_invalid_utf8` | the defaults are v2's |
| `SetEscapeHTML` | `style::escape_html` | off by default, as in v2 |
| x/exp `jsonpointer` | `at_path`, `set_path` | RFC 6901 |
| `json.Valid` | `json::reader(text).skip()` and no `more()` | [`json::reader`](json_reader.md) |

## See also

[`field_list`](fields.md), the types of the program; [`json::reader`](json_reader.md), [`json::writer`](json_writer.md); [`error`](error.md); [`string`](../core/string.md).
