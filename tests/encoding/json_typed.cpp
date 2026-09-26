//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Typed JSON (E4): the types of the program described by describe(), both
// ways — every kind a field may have, the options, the errors with their
// paths, the two doors (to_json, to_text), the reader and the writer of
// typed values, and the text against Go's Marshal of the same structure.
#include "json_common.h"
#include "sgcl/immutable/immutable.h"

#include <cmath>
#include <deque>
#include <map>
#include <thread>

using namespace json_test;
using sgcl::encoding::errc;
using sgcl::encoding::field_list;
using sgcl::string;

namespace typed {
    enum class color { red, green, blue };
    enum class level : uint8_t { low = 1, high = 2 };

    // a text of its own: to_text and from_text
    struct code {
        int a = 0, b = 0;
        string to_text() const {
            return string(std::to_string(a) + "-" + std::to_string(b));
        }
        static sgcl::optional<code> from_text(const string& t) {
            auto dash = t.view().find('-');
            if (dash == std::string_view::npos) {
                return sgcl::nullopt;
            }
            auto a = sgcl::parse<int>(t.view().substr(0, dash));
            auto b = sgcl::parse<int>(t.view().substr(dash + 1));
            if (!a || !b) {
                return sgcl::nullopt;
            }
            return code{*a, *b};
        }
        friend bool operator==(const code&, const code&) = default;
        friend bool operator<(const code& x, const code& y) {
            return x.a < y.a || (x.a == y.a && x.b < y.b);
        }
    };

    // JSON of its own: [x, y]
    struct point {
        double x = 0, y = 0;
        json to_json() const {
            return json::array({x, y});
        }
        static sgcl::optional<point> from_json(const json& j) {
            if (j.size() != 2 || !j[0].as_double() || !j[1].as_double()) {
                return sgcl::nullopt;
            }
            return point{*j[0].as_double(), *j[1].as_double()};
        }
        friend bool operator==(const point&, const point&) = default;
    };

    struct address {
        string city;
        std::string street;
        void describe(field_list& f) {
            f.add("city", city);
            f.add("street", street);
        }
        friend bool operator==(const address&, const address&) = default;
    };

    struct base {
        int64_t id = 0;
        void describe(field_list& f) {
            f.add("id", id);
        }
    };

    struct circle {
        double r = 0;
        void describe(field_list& f) {
            f.add("r", r);
        }
        friend bool operator==(const circle&, const circle&) = default;
    };

    struct square {
        double side = 0;
        void describe(field_list& f) {
            f.add("side", side);
        }
        friend bool operator==(const square&, const square&) = default;
    };

    // every kind a field may have
    struct everything : base {
        bool flag = false;
        int8_t i8 = 0;
        uint8_t u8 = 0;
        int16_t i16 = 0;
        uint32_t u32 = 0;
        int64_t i64 = 0;
        uint64_t u64 = 0;
        float f = 0;
        double d = 0;
        string s;
        std::string ss;
        json raw;
        sgcl::optional<int> maybe;
        sgcl::tracked_ptr<address> where;
        sgcl::vector<string> v;
        sgcl::array<int, 3> a{};
        std::array<int, 2> sa{};
        sgcl::dynamic_array<int> da;
        sgcl::deque<int> dq;
        sgcl::list<string> ls;
        sgcl::immutable::vector<int> iv;
        sgcl::immutable::list<int> il;
        sgcl::map<string, int> m;
        sgcl::map<int, string> mi;
        sgcl::sorted_map<string, double> sm;
        sgcl::ordered_map<string, int> om;
        sgcl::immutable::map<string, int> im;
        sgcl::set<int> st;
        sgcl::sorted_set<string> sst;
        sgcl::immutable::set<int> ist;
        std::pair<int, string> pr;
        std::tuple<bool, double, string> tp;
        sgcl::variant<circle, square> shape;
        color c = color::red;
        level lv = level::low;
        code cd;
        point pt;
        sgcl::sorted_map<code, int> by_code;
        address home;
        std::vector<int> stdv;
        std::map<std::string, int> stdm;
        void describe(field_list& f) {
            base::describe(f);
            f.add("flag", flag);
            f.add("i8", i8);
            f.add("u8", u8);
            f.add("i16", i16);
            f.add("u32", u32);
            f.add("i64", i64);
            f.add("u64", u64);
            f.add("f", this->f);
            f.add("d", d);
            f.add("s", s);
            f.add("ss", ss);
            f.add("raw", raw);
            f.add("maybe", maybe);
            f.add("where", where);
            f.add("v", v);
            f.add("a", a);
            f.add("sa", sa);
            f.add("da", da);
            f.add("dq", dq);
            f.add("ls", ls);
            f.add("iv", iv);
            f.add("il", il);
            f.add("m", m);
            f.add("mi", mi);
            f.add("sm", sm);
            f.add("om", om);
            f.add("im", im);
            f.add("st", st);
            f.add("sst", sst);
            f.add("ist", ist);
            f.add("pr", pr);
            f.add("tp", tp);
            f.add("shape", shape).tagged("type", {"circle", "square"});
            f.add("c", c).names({"red", "green", "blue"});
            f.add("lv", lv);
            f.add("cd", cd);
            f.add("pt", pt);
            f.add("by_code", by_code);
            f.add("home", home);
            f.add("stdv", stdv);
            f.add("stdm", stdm);
        }
    };

    struct node {
        int value = 0;
        sgcl::tracked_ptr<node> next;
        void describe(field_list& f) {
            f.add("value", value);
            f.add("next", next);
        }
    };

    struct person {
        string name;
        int age = 0;
        sgcl::optional<address> home;
        sgcl::tracked_ptr<person> manager;
        void describe(field_list& f) {
            f.add("name", name).required();
            f.add("age", age);
            f.add("home", home);
            f.add("manager", manager);
        }
    };

    struct empties {
        int i = 0;
        bool b = false;
        double d = 0;
        string s;
        sgcl::vector<int> v;
        sgcl::optional<int> o;
        sgcl::tracked_ptr<address> p;
        sgcl::map<string, int> m;
        address rec;   // empty when it compares equal to its default
        void describe(field_list& f) {
            f.add("i", i).omit_empty();
            f.add("b", b).omit_empty();
            f.add("d", d).omit_empty();
            f.add("s", s).omit_empty();
            f.add("v", v).omit_empty();
            f.add("o", o).omit_empty();
            f.add("p", p).omit_empty();
            f.add("m", m).omit_empty();
            f.add("rec", rec).omit_empty();
        }
    };

    struct quoted {
        int64_t n = 0;
        double d = 0;
        bool b = false;
        sgcl::optional<uint16_t> o;
        void describe(field_list& f) {
            f.add("n", n).quoted();
            f.add("d", d).quoted();
            f.add("b", b).quoted();
            f.add("o", o).quoted();
        }
    };

    // a type of another library, described by a free function
    namespace other {
        struct vec2 {
            float x = 0, y = 0;
        };
        inline void describe(field_list& f, vec2& v) {
            f.add("x", v.x);
            f.add("y", v.y);
        }
    }

    // the structure Go marshals in tools/json_oracle.go (goRecord)
    struct go_record {
        string name;
        int32_t count = 0;
        float ratio = 0;
        double big = 0;
        sgcl::vector<int64_t> list;
        sgcl::map<string, int> scores;
        sgcl::map<int, string> by_number;
        sgcl::optional<string> nothing;
        sgcl::vector<float> floats;
        bool ok = false;
        void describe(field_list& f) {
            f.add("name", name);
            f.add("count", count);
            f.add("ratio", ratio);
            f.add("big", big);
            f.add("list", list);
            f.add("scores", scores);
            f.add("by_number", by_number);
            f.add("nothing", nothing);
            f.add("floats", floats);
            f.add("ok", ok);
        }
    };
}

using namespace typed;

namespace {
    everything sample() {
        everything e;
        e.id = 42;
        e.flag = true;
        e.i8 = -128;
        e.u8 = 255;
        e.i16 = -300;
        e.u32 = 4000000000u;
        e.i64 = INT64_MIN;
        e.u64 = UINT64_MAX;
        e.f = 0.1f;
        e.d = 1e21;
        e.s = "zażółć \"gęślą\"\n";
        e.ss = "std";
        e.raw = json::parse(text(R"({"any": [1, "two", null]})")).value();
        e.maybe = 7;
        e.where = sgcl::make_tracked<address>(address{"Kraków", "Floriańska"});
        e.v = {"x", "y"};
        e.a = {1, 2, 3};
        e.sa = {4, 5};
        e.da = sgcl::dynamic_array<int>{6, 7};
        e.dq = {8};
        e.ls = {"l1", "l2"};
        e.iv = e.iv.push_back(9).push_back(10);
        e.il = e.il.push_front(12).push_front(11);
        e.m = {{"one", 1}, {"two", 2}, {"three", 3}};
        e.mi = {{10, "ten"}, {9, "nine"}};
        e.sm = {{"b", 2.5}, {"a", 1.5}};
        e.om = {{"z", 1}, {"a", 2}};
        e.im = e.im.insert("k", 1).insert("j", 2);
        e.st = {3, 1, 2};
        e.sst = {"b", "a"};
        e.ist = e.ist.insert(5).insert(4);
        e.pr = {1, "one"};
        e.tp = {true, 2.5, "t"};
        e.shape = square{3};
        e.c = color::blue;
        e.lv = level::high;
        e.cd = code{1, 2};
        e.pt = point{1.5, -2};
        e.by_code = {{code{3, 4}, 34}};
        e.home = address{"Gdańsk", "Długa"};
        e.stdv = {1, 2};
        e.stdm = {{"k", 1}};
        return e;
    }
}

// Every kind both ways: written, read back, written again the same; the
// text names the order a hash map has none of (sorted)
TEST(JsonTyped_Tests, EveryKindBothWays) {
    auto e = sample();
    auto t = json::stringify(e);
    ASSERT_TRUE(t) << t.error().message();
    EXPECT_EQ(t->view(),
        R"({"id":42,"flag":true,"i8":-128,"u8":255,"i16":-300,"u32":4000000000,"i64":-9223372036854775808,"u64":18446744073709551615,)"
        R"("f":0.1,"d":1e+21,"s":"zażółć \"gęślą\"\n","ss":"std","raw":{"any":[1,"two",null]},"maybe":7,"where":{"city":"Kraków","street":"Floriańska"},)"
        R"("v":["x","y"],"a":[1,2,3],"sa":[4,5],"da":[6,7],"dq":[8],"ls":["l1","l2"],"iv":[9,10],"il":[11,12],)"
        R"("m":{"one":1,"three":3,"two":2},"mi":{"10":"ten","9":"nine"},"sm":{"a":1.5,"b":2.5},"om":{"z":1,"a":2},"im":{"j":2,"k":1},)"
        R"("st":[1,2,3],"sst":["a","b"],"ist":[4,5],"pr":[1,"one"],"tp":[true,2.5,"t"],"shape":{"type":"square","side":3},)"
        R"("c":"blue","lv":2,"cd":"1-2","pt":[1.5,-2],"by_code":{"3-4":34},"home":{"city":"Gdańsk","street":"Długa"},"stdv":[1,2],"stdm":{"k":1}})");
    auto back = json::parse<everything>(*t);
    ASSERT_TRUE(back) << back.error().message();
    auto again = json::stringify(*back);
    ASSERT_TRUE(again);
    EXPECT_EQ(again->view(), t->view());
    EXPECT_EQ(back->where->city, sgcl::string("Kraków"));
    EXPECT_EQ(back->f, 0.1f);
    EXPECT_EQ(back->u64, UINT64_MAX);
    EXPECT_EQ(back->i64, INT64_MIN);
    EXPECT_EQ(std::get<square>(std::variant<circle, square>(square{3})).side, 3.0);
    EXPECT_EQ(back->shape.index(), 1u);
    EXPECT_EQ(back->c, color::blue);
    EXPECT_EQ(back->lv, level::high);
    EXPECT_EQ(back->cd, (code{1, 2}));
    EXPECT_EQ(back->pt, (point{1.5, -2}));
    EXPECT_EQ(back->by_code.at(code{3, 4}), 34);
    EXPECT_EQ(back->raw["any"][1].as_string(), sgcl::string("two"));
    // the same through a tree
    auto tree = json::parse(*t).value();
    auto viatree = tree.as<everything>();
    ASSERT_TRUE(viatree) << viatree.error().message();
    EXPECT_EQ(json::stringify(*viatree)->view(), t->view());
    // pretty
    auto p = json::stringify(address{"A", "B"}, json::pretty);
    EXPECT_EQ(p->view(), "{\n  \"city\": \"A\",\n  \"street\": \"B\"\n}");
    // a type of another library
    auto v = json::parse<other::vec2>(text(R"({"x": 1.5, "y": 2})")).value();
    EXPECT_EQ(v.y, 2.0f);
    EXPECT_EQ(json::stringify(v)->view(), R"({"x":1.5,"y":2})");
    // values that are not records
    EXPECT_EQ(json::stringify(sgcl::vector<int>{1, 2})->view(), "[1,2]");
    using by_name = sgcl::map<string, sgcl::vector<int>>;
    EXPECT_EQ(json::parse<by_name>(text(R"({"a": [1], "b": []})"))->at("a")[0], 1);
    EXPECT_EQ(json::stringify(3)->view(), "3");
    EXPECT_EQ(json::parse<double>(text("2.5")), 2.5);
}

// The text of a structure as Go's json.Marshal writes the same one: the
// order of the fields, the map keys sorted, a float32 with its own digits
TEST(JsonTyped_Tests, WrittenAsGoWritesIt) {
    go_record r;
    r.name = "record";
    r.count = -7;
    r.ratio = 0.3f;
    r.big = 12345678901234567890.0;
    r.list = {1, -2, 3};
    r.scores = {{"zeta", 1}, {"alpha", 2}, {"mid", 3}};
    r.by_number = {{10, "ten"}, {2, "two"}, {33, "thirty-three"}};
    r.floats = {1.1f, 16777216.0f, 3.4028235e38f, 1e-45f};
    r.ok = true;
    EXPECT_EQ(json::stringify(r)->view(), json_oracle::go_record_compact);
    EXPECT_EQ(json::stringify(r, json::pretty)->view(), json_oracle::go_record_indented);
}

namespace {
    // A name in a character array longer than the name: measured to its
    // NUL, not taken as the array's length as a literal is
    struct named_by_buffer {
        int64_t id = 5;
        bool on = true;
        void describe(field_list& f) {
            static const char key[16] = "id";
            f.add(key, id);
            f.add("on", on);
        }
    };
}

TEST(JsonTyped_Tests, FieldNamesFromArrays) {
    EXPECT_EQ(json::stringify(named_by_buffer())->view(), std::string_view(R"({"id":5,"on":true})"));
    auto back = json::parse<named_by_buffer>(sgcl::string(R"({"id":9,"on":false})"));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->id, 9);
    EXPECT_FALSE(back->on);
}

// Where it failed, as a path, and what
TEST(JsonTyped_Tests, ErrorsAndPaths) {
    struct Case { std::string_view text; errc code; std::string_view message; };
    const Case cases[] = {
        {R"({"name": "a", "manager": {"name": "b", "manager": {"name": "c", "age": "x"}}})", errc::type_mismatch, "1:72 /manager/manager/age: expected an integer, found a string"},
        {R"({"name": "a", "home": {"city": 5}})", errc::type_mismatch, "1:32 /home/city: expected a string, found a number"},
        {R"({"age": 1})", errc::missing_field, "1:10 /name: missing field"},
        {R"({"name": "a", "manager": {}})", errc::missing_field, "1:27 /manager/name: missing field"},
        {R"({"name": "a", "age": 1.5})", errc::type_mismatch, "1:22 /age: expected an integer, found 1.5"},
        {R"({"name": "a", "age": 1e10})", errc::out_of_range, "1:22 /age: the number 1e10 is out of the field's range"},
        {R"({"name": "a", "name": "b"})", errc::duplicate_key, "1:15: duplicate key \"name\""},
        {R"({"name": "a", "home": [1]})", errc::type_mismatch, "1:23 /home: expected an object, found an array"},
        {R"({"name": "a", "age": 1,})", errc::syntax, "1:24: invalid character '}' where a key was expected"},
        {R"({"name": "a"} x)", errc::syntax, "1:15: invalid character 'x' after the value"},
        {R"([])", errc::type_mismatch, "1:1: expected an object, found an array"},
    };
    for (auto& c : cases) {
        auto r = json::parse<person>(text(c.text));
        ASSERT_FALSE(r) << c.text;
        EXPECT_EQ(r.error().code(), c.code) << c.text;
        EXPECT_EQ(r.error().message(), sgcl::string(c.message)) << c.text;
    }
    auto seq = json::parse<sgcl::vector<sgcl::map<string, int8_t>>>(text(R"([{"a": 1}, {"b": 2, "c": 300}])"));
    EXPECT_EQ(seq.error().message(), sgcl::string("1:26 /1/c: the number 300 is out of the field's range"));
    auto key = json::parse<sgcl::map<int, int>>(text(R"({"1": 1, "x": 2})"));
    EXPECT_EQ(key.error().code(), errc::type_mismatch);
    EXPECT_EQ(key.error().offset(), 9u);
    auto code_key = json::parse<sgcl::sorted_map<code, int>>(text(R"({"1-2": 1, "12": 2})"));
    EXPECT_EQ(code_key.error().code(), errc::type_mismatch);
    using two = sgcl::array<int, 2>;
    auto fixed = json::parse<two>(text("[1, 2, 3]"));
    EXPECT_EQ(fixed.error().message(), sgcl::string("1:1: expected an array of 2 elements"));
    using both = std::pair<int, int>;
    EXPECT_EQ(json::parse<two>(text("[1]")).error().code(), errc::type_mismatch);
    EXPECT_EQ(json::parse<both>(text("[1, \"a\"]")).error().message(), sgcl::string("1:5 /1: expected an integer, found a string"));
    EXPECT_EQ(json::parse<color>(text("\"red\"")).error().code(), errc::type_mismatch);   // no names without a field
    EXPECT_EQ(json::parse<code>(text("\"12\"")).error().message(), sgcl::string("1:1: \"12\" is not a valid value of the field"));
    EXPECT_EQ(json::parse<point>(text("[1]")).error().code(), errc::type_mismatch);
    // the same errors through a tree: the path, no place in a text
    auto tree = json::parse(text(R"({"name": "a", "manager": {"name": "b", "age": "x"}})")).value();
    auto viatree = tree.as<person>();
    EXPECT_EQ(viatree.error().code(), errc::type_mismatch);
    EXPECT_EQ(viatree.error().path(), sgcl::string("/manager/age"));
    // unknown fields: skipped, or refused
    EXPECT_TRUE(json::parse<person>(text(R"({"name": "a", "extra": {"deep": [1, {"x": null}]}})")));
    json::options strict;
    strict.reject_unknown_fields = true;
    auto unknown = json::parse<person>(text(R"({"name": "a", "extra": 1})"), strict);
    EXPECT_EQ(unknown.error().code(), errc::unknown_field);
    EXPECT_EQ(unknown.error().offset(), 14u);
    EXPECT_EQ(tree.as<person>(strict).error().code(), errc::type_mismatch);
    // a key twice inside a value skipped: refused as parse refuses it
    EXPECT_EQ(json::parse<person>(text(R"({"name": "a", "extra": {"k": 1, "k": 2}})")).error().code(), errc::duplicate_key);
}

// Variants by their tag, wherever it is; errors of the tag
TEST(JsonTyped_Tests, Variants) {
    struct drawing {
        sgcl::vector<sgcl::variant<circle, square>> shapes;
        void describe(field_list& f) {
            f.add("shapes", shapes).tagged("kind", {"circle", "square"});
        }
    };
    auto d = json::parse<drawing>(text(R"({"shapes": [{"kind": "circle", "r": 1}, {"side": 2, "kind": "square"}]})"));
    ASSERT_TRUE(d) << d.error().message();
    EXPECT_EQ(std::get<0>(std::variant<circle, square>(circle{})).r, 0);
    EXPECT_EQ(d->shapes.size(), 2u);
    EXPECT_EQ(d->shapes[0].index(), 0u);
    EXPECT_EQ(sgcl::get<circle>(d->shapes[0]).r, 1.0);
    EXPECT_EQ(sgcl::get<square>(d->shapes[1]).side, 2.0);
    EXPECT_EQ(json::stringify(*d)->view(), R"({"shapes":[{"kind":"circle","r":1},{"kind":"square","side":2}]})");
    EXPECT_EQ(json::parse<drawing>(text(R"({"shapes": [{"r": 1}]})")).error().message(), sgcl::string("1:20 /shapes/0/kind: missing field"));
    EXPECT_EQ(json::parse<drawing>(text(R"({"shapes": [{"kind": "hexagon"}]})")).error().message(), sgcl::string("1:22 /shapes/0/kind: \"hexagon\" is none of the alternatives"));
    EXPECT_EQ(json::parse<drawing>(text(R"({"shapes": [{"kind": "circle", "r": "x"}]})")).error().path(), sgcl::string("/shapes/0/r"));
    auto tree = json::parse(text(R"({"shapes": [{"side": 5, "kind": "square"}]})")).value();
    EXPECT_EQ(sgcl::get<square>(tree.as<drawing>()->shapes[0]).side, 5.0);
    // a variant with no tag given to its field cannot be written or read
    struct untagged {
        sgcl::variant<circle, square> s;
        void describe(field_list& f) {
            f.add("s", s);
        }
    };
    EXPECT_EQ(json::stringify(untagged{}).error().code(), errc::unsupported_value);
    EXPECT_EQ(json::parse<untagged>(text(R"({"s": {}})")).error().code(), errc::unsupported_value);
}

TEST(JsonTyped_Tests, OmitEmptyAndAsString) {
    EXPECT_EQ(json::stringify(empties{})->view(), "{}");
    empties full;
    full.i = 1;
    full.b = true;
    full.d = 0.5;
    full.s = "s";
    full.v = {1};
    full.o = 0;
    full.p = sgcl::make_tracked<address>();
    full.m = {{"k", 0}};
    full.rec.city = "c";
    EXPECT_EQ(json::stringify(full)->view(), R"({"i":1,"b":true,"d":0.5,"s":"s","v":[1],"o":0,"p":{"city":"","street":""},"m":{"k":0},"rec":{"city":"c","street":""}})");
    quoted q{-12, 2.5, true, 7};
    auto t = json::stringify(q);
    EXPECT_EQ(t->view(), R"({"n":"-12","d":"2.5","b":"true","o":"7"})");
    auto back = json::parse<quoted>(*t).value();
    EXPECT_EQ(back.n, -12);
    EXPECT_EQ(back.d, 2.5);
    EXPECT_TRUE(back.b);
    EXPECT_EQ(back.o, uint16_t(7));
    EXPECT_EQ(json::parse<quoted>(text(R"({"n": 12})")).error().code(), errc::type_mismatch);
    EXPECT_EQ(json::parse<quoted>(text(R"({"n": "12x"})")).error().code(), errc::type_mismatch);
    EXPECT_EQ(json::parse<quoted>(text(R"({"o": "70000"})")).error().code(), errc::out_of_range);
    EXPECT_EQ(json::parse<quoted>(text(R"({"b": "yes"})")).error().code(), errc::type_mismatch);
}

// Numbers into typed fields: exactly or not at all, each type rounded once
TEST(JsonTyped_Tests, Numbers) {
    // an integer field takes a number whose value is an integer: 1.0 and
    // 1e2 are 1 and 100 (Go's v2 refuses both: a difference by name)
    EXPECT_EQ(json::parse<int>(text("1.0")), 1);
    EXPECT_EQ(json::parse<int>(text("1e2")), 100);
    EXPECT_EQ(json::parse<int>(text("-0")), 0);
    EXPECT_EQ(json::parse<int>(text("1.5")).error().code(), errc::type_mismatch);
    EXPECT_EQ(json::parse<uint8_t>(text("256")).error().code(), errc::out_of_range);
    EXPECT_EQ(json::parse<uint8_t>(text("-1")).error().code(), errc::out_of_range);
    EXPECT_EQ(json::parse<int8_t>(text("-128")), int8_t(-128));
    EXPECT_EQ(json::parse<int64_t>(text("9223372036854775808")).error().code(), errc::out_of_range);
    EXPECT_EQ(json::parse<uint64_t>(text("18446744073709551615")), UINT64_MAX);
    EXPECT_EQ(json::parse<uint64_t>(text("1e30")).error().code(), errc::out_of_range);
    // a float is rounded from the decimal, never from a double: this
    // literal is a hair over the halfway point between 1 and the next
    // float; a double rounds it onto the halfway point, and the float from
    // that double is 1 (the tie to even) where the right one is the next
    auto f = json::parse<float>(text("1.00000005960464477539062501"));
    EXPECT_EQ(*f, 1.00000012f);
    EXPECT_EQ(float(1.00000005960464477539062501), 1.0f);   // the double's way: rounded twice, wrong
    EXPECT_EQ(json::parse<float>(text("3.4028236e38")).error().code(), errc::out_of_range);
    EXPECT_EQ(json::parse<double>(text("1e400")).error().code(), errc::out_of_range);
    EXPECT_EQ(json::parse<double>(text("1e-400")), 0.0);
    // written: a float's own digits, NaN refused with its path
    struct floats {
        float f = 0.1f;
        double d = 0;
        void describe(field_list& fl) {
            fl.add("f", f);
            fl.add("d", d);
        }
    };
    EXPECT_EQ(json::stringify(floats{})->view(), R"({"f":0.1,"d":0})");
    auto nan = json::stringify(floats{0.1f, std::nan("")});
    EXPECT_EQ(nan.error().code(), errc::unsupported_value);
    EXPECT_EQ(nan.error().path(), sgcl::string("/d"));
    EXPECT_EQ(json::stringify(sgcl::vector<double>{1, INFINITY}).error().path(), sgcl::string("/1"));
    // null into a field that is not optional: its default (Go's v2)
    auto p = json::parse<person>(text(R"({"name": "n", "age": null, "home": null})")).value();
    EXPECT_EQ(p.age, 0);
    EXPECT_FALSE(p.home);
    auto e = json::parse<everything>(text(R"({"v": null, "m": null, "home": null, "c": null})")).value();
    EXPECT_TRUE(e.v.empty());
    EXPECT_EQ(e.c, color::red);
}

// An enum's name that is not there, a value past the names
TEST(JsonTyped_Tests, Enums) {
    struct paint {
        color c = color::red;
        void describe(field_list& f) {
            f.add("c", c).names({"red", "green", "blue"});
        }
    };
    EXPECT_EQ(json::parse<paint>(text(R"({"c": "green"})"))->c, color::green);
    EXPECT_EQ(json::parse<paint>(text(R"({"c": "pink"})")).error().code(), errc::type_mismatch);
    EXPECT_EQ(json::parse<paint>(text(R"({"c": 1})")).error().code(), errc::type_mismatch);
    EXPECT_EQ(json::stringify(paint{color(7)}).error().code(), errc::unsupported_value);
    EXPECT_EQ(json::stringify(level::high)->view(), "2");
    EXPECT_EQ(json::parse<level>(text("256")).error().code(), errc::out_of_range);
}

// A cycle when writing is found by the depth, with its path; a list 512
// long is read on a thread with macOS's 512 KB stack
TEST(JsonTyped_Tests, CyclesAndDepth) {
    sgcl::tracked_ptr<node> a = sgcl::make_tracked<node>();
    a->next = a;
    auto cycle = json::stringify(*a);
    EXPECT_EQ(cycle.error().code(), errc::unsupported_value);
    EXPECT_TRUE(cycle.error().path().view().starts_with("/next/next/next"));
    std::string deep;
    for (int i = 0; i < 511; ++i) {
        deep += "{\"value\": " + std::to_string(i) + ", \"next\": ";
    }
    deep += "null" + std::string(511, '}');
    bool ok = false;
    size_t length = 0;
    std::thread t([&] {
        auto r = json::parse<node>(text(deep));
        ok = r.has_value();
        for (auto p = r ? r->next : nullptr; p; p = p->next) {
            ++length;
        }
    });
    t.join();
    EXPECT_TRUE(ok);
    EXPECT_EQ(length, 510u);
    std::string too_deep;
    for (int i = 0; i < 513; ++i) {
        too_deep += "{\"next\": ";
    }
    too_deep += "null" + std::string(513, '}');
    EXPECT_EQ(json::parse<node>(text(too_deep)).error().code(), errc::depth_limit);
}

// Records one at a time from a stream, and written into one
TEST(JsonTyped_Tests, ReaderAndWriter) {
    std::string lines;
    for (int i = 0; i < 200; ++i) {
        lines += "{\"name\": \"p" + std::to_string(i) + "\", \"age\": " + std::to_string(i) + ", \"home\": {\"city\": \"c\"}}\n";
    }
    lines += "{\"age\": 5}\n";
    for (size_t n : {0, 1, 7, 100}) {
        sgcl::tracked_ptr<json::reader> r = n ? sgcl::make_tracked<json::reader>(sgcl::make_tracked<dribble>(lines, n)) : sgcl::make_tracked<json::reader>(text(lines));
        int i = 0;
        while (r->more()) {
            auto p = r->read<person>();
            if (!p) {
                break;
            }
            EXPECT_EQ(p->age, i);
            EXPECT_EQ(p->home->city, sgcl::string("c"));
            ++i;
        }
        EXPECT_EQ(i, 200);
        ASSERT_TRUE(r->last_error());
        EXPECT_EQ(r->last_error()->code(), errc::missing_field);
        EXPECT_EQ(r->last_error()->path(), sgcl::string("/name"));
        EXPECT_EQ(r->last_error()->line(), 201u);
    }
    // a stream parsed whole as a T
    auto whole = json::parse<sgcl::vector<int>>(sgcl::make_tracked<dribble>(std::string("[1, 2, 3]"), 2));
    EXPECT_EQ(whole->size(), 3u);
    // the writer: typed values and the rest mixed
    sgcl::tracked_ptr out = sgcl::make_tracked<sink>();
    json::writer w(out);
    w.value(address{"a", "b"}).begin_array().value(person{"x", 1, {}, {}}).value(sgcl::vector<int>{1}).end_array();
    EXPECT_TRUE(w.flush());
    EXPECT_EQ(out->text, "{\"city\":\"a\",\"street\":\"b\"}\n[{\"name\":\"x\",\"age\":1,\"home\":null,\"manager\":null},[1]]\n");
    sgcl::tracked_ptr bad = sgcl::make_tracked<sink>();
    json::writer wb(bad);
    wb.value(sgcl::vector<double>{1, NAN});
    auto f = wb.flush();
    ASSERT_FALSE(f);
    EXPECT_EQ(f.error().message(), sgcl::string("json: /1: NaN is not a JSON number: unsupported value"));
    // in a task
    auto t = sgcl::async::spawn([](std::string lines) -> sgcl::async::task<int> {
        json::reader r(sgcl::make_tracked<dribble>(lines, 5));
        int i = 0;
        while (co_await r.async_more()) {
            auto p = co_await r.async_read<person>();
            if (!p) {
                break;
            }
            ++i;
        }
        auto whole = co_await json::async_parse<sgcl::vector<int>>(sgcl::make_tracked<dribble>(std::string("[4, 5]"), 1));
        co_return i == 200 && whole && whole->size() == 2 ? 1 : 0;
    }(lines));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// json::from<T>: the value of a T, as xml::from makes an element of one
TEST(JsonTyped_Tests, AValueFromAT) {
    address a{string("Kraków"), "Floriańska"};
    auto j = json::from(a);
    ASSERT_TRUE(j);
    EXPECT_EQ((*j)["city"].as_string(), string("Kraków"));
    EXPECT_EQ(j->as<address>().value(), a);                  // and back
    auto n = json::from(42);
    ASSERT_TRUE(n);
    EXPECT_EQ(n->as_int(), 42);
    auto nan = json::from(std::numeric_limits<double>::quiet_NaN());
    EXPECT_FALSE(nan);                                        // what stringify refuses, from refuses
}
