//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/function.h"
#include "../core/root_ptr.h"
#include "../core/string.h"
#include "../core/variant.h"
#include "../core/ordered_map.h"
#include "../core/vector.h"
#include "case.h"
#include "format.h"

#include <algorithm>

// A text written from a pattern that the program did not write: a page, a
// letter, a report, whose shape lives in a file and is changed by whoever
// owns the words rather than by whoever owns the program. The shape is
// text with actions in it — {{ name }}, {{ if }}, {{ range }} — and the
// values that go in are handed over by the caller.
//
// Why this is here and not somewhere of its own. A template writes
// values, and writing a value is what format.h does: the field after the
// colon is the format_spec of that header, read by its read_spec and
// written by its writers, so {{ total:>8.2f }} pads and rounds exactly as
// format("{:>8.2f}", total) does, and the field is measured in the
// columns the text takes rather than in its bytes. Nothing about writing
// a number or padding a field is written a second time here; this header
// adds the walking of a shape, and nothing else.
//
// The one place that is not true, and it is a colon. A pattern knows the
// type of every value where it is read, so it knows whether a second
// colon opens a specification for what the value holds or is merely a
// character to pad with: format("{::>4}", 7) is a seven padded to four
// with colons, because a number holds nothing. A template knows no types
// at all — the values arrive long after the source is read — so the
// second colon is always read as opening one, a value being able to hold
// other values, and where it turns out not to the nested part is dropped
// and the rest of the field kept. So {{ n::>4 }} over 7 writes 7 and not
// :::7. A fill of colons is the whole of what is lost, and writing it
// another way costs nothing: {{ n:c>4 }}, or any other character.
//
// The two halves and why they are two. A template is read once and
// written many times — that is the whole of what it is for — so parsing
// and rendering are separate calls with the compiled form in between:
//
//     auto t = txt::stencil::parse(source);
//     if (!t) { /* the source is not a template */ }
//     string page = t->render(data);
//
// Nothing throws. A source that does not parse is a thing that really
// happens — a file somebody edited — so it is an answer and not an
// exception, as a pattern read where the program runs is in format.h:
// parse gives back an optional, and the overload taking a
// stencil_error says where and why it stopped.
//
// Where the values come from. C++20 has no reflection, so this cannot
// walk the fields of a struct of the caller's and does not pretend to:
// the caller builds a value, which is text, a number, a truth, nothing,
// a list, or a mapping of names to more of the same, and writes it with
// braces so that the call reads as data:
//
//     txt::value data = txt::object{
//         {"user", txt::object{{"name", "Ada"}, {"admin", true}}},
//         {"scores", txt::list{91.5, 88.0}},
//     };
//
// What this deliberately does not do, and it is a boundary and not a gap.
// There is no escaping that knows where a value lands. Go has that in
// html/template — it reads the HTML around a field and escapes for an
// attribute, a URL or a script accordingly — and it is a parser for a
// second language and a promise this module is not in a position to
// keep. What is here is escape_html as a function of the pipeline, asked
// for by name; whoever writes the template decides where it is needed,
// and the safety of the HTML is the caller's. A boundary written down is
// worth more than one discovered.
namespace sgcl::txt {
    namespace detail {
        struct value_list;
        struct value_object;

        // How the renderer walks what a value holds without any of it
        // becoming part of what a caller may reach for. A template needs
        // the elements of a list and the names of a mapping in order;
        // nobody else does, and a public way of asking would be a
        // promise about the shape of the thing rather than about what it
        // holds.
        struct value_reach;
    }

    // What a value is. Scoped, so that `list` and `object` here and the
    // two classes of those names below do not have to fight over the
    // words: a value_kind::list is what a txt::list makes.
    enum class value_kind : uint8_t {
        none,
        boolean,
        integer,
        real,
        text,
        list,
        object,
    };

    // One value handed to a template. Seven shapes, held in a variant of
    // the library — which is what keeps the two pointers of the compound
    // shapes at a fixed word of their own, where the collector looks —
    // and the two that hold other values reach them through a
    // tracked_ptr, since a value that held a vector of values by
    // membership would be a type of infinite size.
    //
    // Thirty-two bytes, the same as a string and a tag: nothing here is
    // allocated for a number or a truth, and text costs what a string of
    // this library costs, which is a pointer to characters that are
    // shared and never copied.
    class value {
    public:
        // Nothing at all, which is also what a name nobody gave answers
        // to: {{ missing }} writes nothing rather than stopping
        value() noexcept = default;

        value(std::nullptr_t) noexcept {
        }

        value(bool v) noexcept
        : _held(v) {
        }

        // Every whole number arrives as the widest one. A char is a byte
        // of UTF-8 and not a small number here, so it is refused rather
        // than quietly written as 65: text is what a piece of text is.
        template<class T>
        requires std::integral<T> && (!std::same_as<std::remove_cv_t<T>, bool>)
                 && (!std::same_as<std::remove_cv_t<T>, char>)
                 && (!std::same_as<std::remove_cv_t<T>, char32_t>)
        value(T v) noexcept
        : _held(static_cast<long long>(v)) {
        }

        template<class T>
        requires std::floating_point<T>
        value(T v) noexcept
        : _held(static_cast<double>(v)) {
        }

        value(const string& v)
        : _held(v) {
        }

        value(string&& v)
        : _held(std::move(v)) {
        }

        value(const char* v)
        : _held(string(v)) {
        }

        value(const slice<const char>& v)
        : _held(string(v)) {
        }

        // A list written as braces holding values, so that one nested
        // inside a mapping needs no type named at all:
        // {"tags", {"one", "two"}}. The trap of the language comes with
        // it and is worth saying out loud: value v{"one"} is a list of
        // one piece of text, where value v("one") is the text — the
        // initializer list wins in copy-list-initialization, as it does
        // everywhere else in C++.
        value(std::initializer_list<value> items);

        value(const vector<value>& items);

        value_kind kind() const noexcept {
            return value_kind(_held.index());
        }

        bool is_none() const noexcept {
            return kind() == value_kind::none;
        }

        // What an `if` and a `with` ask, and what `default` calls empty.
        // The rule is the one a reader of Go or of Python expects:
        // nothing, false, a number that is nought, text with no
        // characters and a list or a mapping with no elements are all
        // false, and everything else is true.
        bool truthy() const noexcept;

        // The value a name stands for inside this one, or null where
        // this is not a mapping or holds no such name. A pointer and not
        // an optional: what comes back lives in the mapping and is not
        // copied, and a template asks this once per field of every row.
        const value* find(const string& name) const noexcept;

        // The n-th element of a list, or null
        const value* at(size_t index) const noexcept;

        // How many elements a list or a mapping holds; nought for
        // everything else, which is what makes `range` over a number
        // take the empty road rather than a wrong one
        size_t size() const noexcept;

        const string* text() const noexcept {
            return get_if<string>(&_held);
        }

        // What this holds, written the way format.h would write it. The
        // specification is the one the template gave, and a value that
        // refuses it — {:d} over a name — is written with the type
        // letter dropped and the field kept, which is the one answer
        // that is total and still honours the column the template asked
        // for. Nothing here is a second implementation of anything:
        // every alternative goes to detail::write_one.
        void write(format_sink& out, const format_spec& spec,
                   std::string_view nested = {}) const;

        // The same as a string, with no specification: what a pipeline
        // function is given when it wants characters (a conversion of
        // any value, hence to_string: json's as_string() is the string a
        // string node holds, and nothing for any other)
        string to_string() const;

        friend struct detail::value_reach;

    protected:
        // What the two classes below reach for. A mapping is built by
        // its own type and nowhere else, so this is theirs and not the
        // world's.
        void _become_object();

        detail::value_object* _as_object() noexcept;

    private:
        variant<monostate, bool, long long, double, string,
                tracked_ptr<detail::value_list>, tracked_ptr<detail::value_object>> _held;
    };

    namespace detail {
        // The two shapes that hold other values. Each is a struct of its
        // own rather than an alias of the container it wraps, so that
        // the pages these are allocated from hold nothing else: a
        // template walking a list of rows touches them one after another
        // and they lie together.
        struct value_list {
            vector<value> items;

            value_list() = default;

            explicit value_list(std::initializer_list<value> il)
            : items(il) {
            }

            explicit value_list(const vector<value>& v)
            : items(v) {
            }
        };

        // Insertion order and not the order of a hash: a template that
        // walks a mapping must write the same page twice running, and
        // the order the caller wrote is the only one anybody can
        // predict.
        struct value_object {
            ordered_map<string, value> fields;

            value_object() = default;

            explicit value_object(std::initializer_list<pair<string, value>> il) {
                for (auto& f : il) {
                    fields.insert_or_assign(f.first, f.second);
                }
            }
        };
    }

    // The two shapes written as data. Each is a value and adds nothing
    // to it — no member, nothing virtual — so it is a value wherever one
    // is wanted and the slicing that gives is the point rather than a
    // hazard. They exist because a brace on its own cannot say which of
    // the two was meant.
    class list : public value {
    public:
        list() = default;

        list(std::initializer_list<value> items)
        : value(items) {
        }

        explicit list(const vector<value>& items)
        : value(items) {
        }
    };

    class object : public value {
    public:
        object();

        object(std::initializer_list<pair<string, value>> fields);

        // Built a field at a time, for a mapping whose names are not
        // known where the program is written
        void set(const string& name, const value& v);
    };

    //--------------------------------------------------------------------
    // Where a source stopped being a template, for whoever has to fix it.
    // A byte offset alone is no use to a person looking at a file, so the
    // line and the column come with it, both counted from one.
    //--------------------------------------------------------------------
    class stencil_error {
    public:
        stencil_error(size_t offset, size_t line, size_t column, const char* reason) noexcept
        : _offset(offset)
        , _line(line)
        , _column(column)
        , _reason(reason) {
        }

        // The byte the reading stopped on
        size_t offset() const noexcept {
            return _offset;
        }

        size_t line() const noexcept {
            return _line;
        }

        size_t column() const noexcept {
            return _column;
        }

        // Why, in a few words
        string message() const {
            return string(_reason);
        }

    private:
        size_t _offset;
        size_t _line;
        size_t _column;
        const char* _reason;
    };

    //--------------------------------------------------------------------
    // A function of the pipeline: {{ name | upper }}, {{ n | default 0 }}.
    // What comes down the pipe is the first argument and whatever was
    // written after the name is the second — literals and paths both,
    // already worked out. A function answers a value, so functions
    // compose and one may hand a list to the next.
    //
    // It must be pure of side effects. A render that does not fit the
    // room it was given writes one step again, so a function may be
    // called twice for the same field; nothing else depends on it.
    //--------------------------------------------------------------------
    using stencil_function = function<value(const value&, slice<const value>)>;

    namespace detail {
        string escape_html_text(const string& text);
    }

    // The names a template may call. Built with the six that are always
    // there and added to by the caller; handed to parse, which resolves
    // every name written in the source against it, so a template calling
    // a function nobody wrote fails to parse rather than writing nothing
    // where a word was wanted.
    class stencil_functions {
    public:
        stencil_functions();

        // Replaces a name already there, which is how a caller overrides
        // one of the six.
        //
        // The function must be pure of side effects, and this is the
        // place to say so rather than only up beside the type: whoever
        // writes one is looking here. A render that does not fit the
        // room it was given writes one step again — one field, into room
        // twice the size, rather than the page — so the pipeline of that
        // field runs a second time, path and all. Which field it is
        // depends on where the page happens to cross a kilobyte and each
        // doubling after it, so a function that counts, logs or reads a
        // clock will be right on most pages and wrong on some, and the
        // some are not the ones anybody tests. Nothing else in a render
        // depends on it.
        void add(const string& name, stencil_function fn) {
            _named.insert_or_assign(name, std::move(fn));
        }

        const stencil_function* find(const string& name) const {
            auto i = _named.find(name);
            return i == _named.end() ? nullptr : &i->second;
        }

        // The six that are always there, shared by every template that
        // does not ask for a table of its own
        static const stencil_functions& builtin();

    private:
        ordered_map<string, stencil_function> _named;
    };

    namespace detail {
        //----------------------------------------------------------------
        // The compiled form. A template is read once and walked many
        // times, so the reading writes down steps and the walking never
        // looks at a character of the source except to copy a run of it
        // out — the same division format.h makes between the pattern the
        // compiler read and the parts it left behind.
        //
        // Where the two differ is what the steps can do. A format
        // pattern is a straight line; a template branches and loops, so
        // a step carries a target and the walk is a program counter
        // rather than a loop over parts.
        //----------------------------------------------------------------
        enum class stencil_op : uint8_t {
            text,       // write the run of source at [a, a + b)
            write,      // work out expression a, write it
            branch,     // work out expression a; if it is false, go to b
            jump,       // go to b
            enter,      // `with`: if expression a is true, make it the dot; else go to b
            leave,      // give the dot back
            loop,       // `range`: open expression a; if it is empty, go to b
            repeat,     // one element on; if there is one, go back to b
        };

        struct stencil_step {
            stencil_op op = stencil_op::text;
            uint32_t a = 0;
            uint32_t b = 0;
        };

        // One thing a field asks for: a path, possibly from the root,
        // possibly a literal, then the functions it is piped through,
        // then how it is to be written.
        struct stencil_expr {
            enum class origin : uint8_t { dot, root, literal };

            origin from = origin::dot;
            uint32_t lit = 0;           // the literal, when from == literal
            uint32_t path_at = 0;       // the first of its names
            uint32_t path_size = 0;
            uint32_t pipe_at = 0;       // the first of its stages
            uint32_t pipe_size = 0;
            format_spec spec;
            // What a second colon handed on to the elements, kept as
            // offsets into the source rather than as a view, so that a
            // compiled template points at nothing that can move. The
            // flag keeps the difference format.h rests on: a nested
            // specification that is there and empty, {::}, is not the
            // same as one that was never written.
            uint32_t nested_at = 0;
            uint32_t nested_size = 0;
            bool has_nested = false;
        };

        // What one field may hand to one function. Past this the
        // template is refused, which keeps the working room of a render
        // on the stack and out of the allocator.
        inline constexpr size_t MaxArgs = 8;

        struct stencil_stage {
            uint32_t fn = 0;            // which of the resolved functions
            uint32_t args_at = 0;       // the first of its arguments
            uint32_t args_size = 0;
        };

        // One open loop: what it walks, where it has got to, and what
        // `owned` held before it opened. Out here rather than inside the
        // walk because the walk is now handed its room instead of
        // standing on it, and the two callers that hand it over have to
        // be able to name the type.
        struct stencil_frame {
            const value_list* items = nullptr;
            const value_object* fields = nullptr;
            decltype(std::declval<const value_object&>().fields.begin()) it {};
            size_t index = 0;
            size_t mark = 0;
        };
    }

    //--------------------------------------------------------------------
    // The template itself: a source read into steps, and a render that
    // walks them.
    //
    // The source is kept rather than copied out of. A string of this
    // library is shared and immutable, so holding it costs a pointer and
    // no characters, and every run of literal text in the page is then a
    // pair of offsets into it and is written out whole.
    //--------------------------------------------------------------------
    class stencil {
    public:
        stencil() = default;

        // The common call, with the six built-in functions and nothing
        // else. What comes back where the source is not a template is the
        // error: where and why the reading stopped.
        static expected<stencil, stencil_error> parse(const string& source) {
            return parse(source, stencil_functions::builtin());
        }

        // The whole of it. `functions` is kept by the template, so a
        // table built on the stack of the caller must outlive it — which
        // the built-in one, being a static, always does.
        static expected<stencil, stencil_error> parse(const string& source,
                                                      const stencil_functions& functions);

        // Whether a source is a template at all, with nothing kept. For
        // a program that reads a directory of them at startup and wants
        // to say which one is broken before it needs any of them.
        static bool parses(const string& source) {
            return parse(source).has_value();
        }

        string render(const value& data) const;

        // The same into room the caller lends: what fits is written and
        // the whole size comes back, whether or not it fitted, so a
        // caller may ask with an empty buffer and then size one. The
        // same contract format_to has.
        size_t render_to(const slice<char>& buffer, const value& data) const;

        // How many steps the source came to — nothing a program needs,
        // and what a test asks to know that a page of text is one step
        // and not four hundred
        size_t steps() const noexcept {
            return _steps.size();
        }

        const string& source() const noexcept {
            return _source;
        }

    private:
        friend struct stencil_parser;

        // How deep the blocks may go before the walk's bookkeeping stops
        // fitting on the stack of _run
        static constexpr size_t InlineDepth = 4;

        void _run(growing_sink& room, const value& data) const;

        // The deep road, which is a call of its own and not a branch
        // inside _run: three managed vectors declared where _run can see
        // them are three objects built and unbuilt on the frame of every
        // render, including the pages — nearly all of them — that never
        // reach this at all.
        SGCL_NOINLINE void _run_deep(growing_sink& room, const value& data) const;

        // The walk itself, over room somebody else is standing on
        void _walk(growing_sink& room, const value& data, const value** dots,
                   detail::stencil_frame* frames, value* owned) const;

        const value* _resolve(const detail::stencil_expr& e, const value* dot,
                              const value* root) const noexcept;

        value _eval(const detail::stencil_expr& e, const value* dot, const value* root) const;

        string _source;
        vector<detail::stencil_step> _steps;
        vector<detail::stencil_expr> _exprs;
        vector<detail::stencil_stage> _stages;
        vector<detail::stencil_expr> _args;
        vector<string> _names;
        vector<value> _literals;
        vector<stencil_function> _functions;
        // How deep the blocks go, worked out where the source is read.
        // A render keeps one frame and one value per open block and
        // reserves both to this before it starts, so nothing it holds a
        // pointer into can move under it and no render allocates for its
        // own bookkeeping at all.
        uint32_t _depth = 0;
    };
}

//------------------------------------------------------------------------------
// The value, whose compound halves could not be defined until the two
// structs they point at were
//------------------------------------------------------------------------------
namespace sgcl::txt {
    inline value::value(std::initializer_list<value> items)
    : _held(make_tracked<detail::value_list>(items)) {
    }

    inline value::value(const vector<value>& items)
    : _held(make_tracked<detail::value_list>(items)) {
    }

    inline void value::_become_object() {
        _held = make_tracked<detail::value_object>();
    }

    inline detail::value_object* value::_as_object() noexcept {
        auto o = get_if<tracked_ptr<detail::value_object>>(&_held);
        return o ? o->get() : nullptr;
    }

    inline object::object() {
        _become_object();
    }

    inline object::object(std::initializer_list<pair<string, value>> fields) {
        _become_object();
        auto o = _as_object();
        for (auto& f : fields) {
            o->fields.insert_or_assign(f.first, f.second);
        }
    }

    inline void object::set(const string& name, const value& v) {
        if (auto o = _as_object()) {
            o->fields.insert_or_assign(name, v);
        }
    }

    inline size_t value::size() const noexcept {
        if (auto l = get_if<tracked_ptr<detail::value_list>>(&_held)) {
            return (*l) ? (*l)->items.size() : 0;
        }
        if (auto o = get_if<tracked_ptr<detail::value_object>>(&_held)) {
            return (*o) ? (*o)->fields.size() : 0;
        }
        return 0;
    }

    inline bool value::truthy() const noexcept {
        switch (kind()) {
            case value_kind::none:    return false;
            case value_kind::boolean: return *get_if<bool>(&_held);
            case value_kind::integer: return *get_if<long long>(&_held) != 0;
            case value_kind::real: {
                double d = *get_if<double>(&_held);
                return d != 0.0;
            }
            case value_kind::text:    return !get_if<string>(&_held)->empty();
            case value_kind::list:
            case value_kind::object:  return size() != 0;
        }
        return false;
    }

    inline const value* value::find(const string& name) const noexcept {
        auto o = get_if<tracked_ptr<detail::value_object>>(&_held);
        if (!o || !*o) {
            return nullptr;
        }
        auto i = (*o)->fields.find(name);
        return i == (*o)->fields.end() ? nullptr : &i->second;
    }

    inline const value* value::at(size_t index) const noexcept {
        auto l = get_if<tracked_ptr<detail::value_list>>(&_held);
        if (!l || !*l || index >= (*l)->items.size()) {
            return nullptr;
        }
        return &(*l)->items[index];
    }
}

//------------------------------------------------------------------------------
// How a value is written, which is the whole of this header's claim to
// live in txt: every road below ends in detail::write_one of format.h
//------------------------------------------------------------------------------
namespace sgcl::txt {
    namespace detail {
        // One alternative written with the specification the template
        // gave, or — where that type refuses it — with as much of it as
        // the type will take.
        //
        // A specification is read where the template is parsed, so it is
        // known to be well formed by the time anything is rendered; what
        // cannot be known there is the type it will meet, the values
        // arriving long after. {:d} over a name is that case. Refusing
        // to render the page would punish the reader for a slip of the
        // template's author, and writing nothing would lose the value
        // altogether, so the type letter is dropped and everything else
        // — the fill, the alignment, the width — is kept, which leaves
        // the column the template asked for intact and the value in it.
        template<class T>
        void write_as(format_sink& out, const T& v, const format_spec& spec,
                      std::string_view nested) {
            if (takes_one<T>(spec, nested)) {
                write_one(out, v, spec, nested);
                return;
            }
            // A value that holds nothing cannot take a specification for
            // its elements, and that is the part to give up first. The
            // type letter in front of it may be the debug form a range
            // gave its elements, and taking the letter away before the
            // nested part wrote a text inside a nested field without its
            // quotes while the text one level down kept them — the same
            // list written two ways at once.
            if (nested.data() && takes_one<T>(spec, std::string_view{})) {
                write_one(out, v, spec, std::string_view{});
                return;
            }
            format_spec plain = spec;
            plain.type = 0;
            if (!takes_one<T>(plain, nested)) {
                plain.precision = -1;
                plain.alternate = false;
                plain.zero = false;
            }
            if (!takes_one<T>(plain, nested)) {
                nested = std::string_view{};
            }
            write_one(out, v, plain, nested);
        }

        // How deep a value may be written into itself.
        //
        // The two compound alternatives hold their elements through a
        // tracked pointer, which is what lets a value be put inside
        // itself — object o; o.set("self", o) is two lines and no cast,
        // the second value sharing the pointer of the first — and then
        // the writing below does not come back. The collector is happy
        // with the cycle, that being what a collector is for; the
        // writer cannot be, having no memory of where it has already
        // been and no room to keep one.
        //
        // So it counts, and past this it writes an ellipsis and returns,
        // which keeps a page total the way every other refusal here
        // does. Sixty-four is past any data a template is written for
        // and far short of what a stack holds: each level is four frames
        // and a few hundred bytes, so the whole of it is under a tenth
        // of the smallest stack this library runs on.
        //
        // Only the two compound alternatives count themselves. A number
        // or a name cannot hold anything, so nothing on the common road
        // reads this at all — which is the point of keeping it here and
        // not in value::write.
        inline constexpr unsigned MaxValueDepth = 64;

        inline thread_local unsigned value_depth = 0;

        struct value_step {
            value_step() noexcept {
                ++value_depth;
            }

            ~value_step() {
                --value_depth;
            }

            explicit operator bool() const noexcept {
                return value_depth <= MaxValueDepth;
            }
        };
    }

    // A value written by format.h itself, which is what lets a list and
    // a mapping come out in the shapes of C++23 without a line of
    // writing here: the range formatter of that header walks a
    // vector<value> and asks this of every element, and the pair
    // formatter writes a name and its value as name: value.
    //
    // It takes whatever it is given, the alternative it holds not being
    // known where a pattern is read; what the alternative will not take
    // is dealt with above, at the moment the value is in hand. It also
    // means txt::format("{}", data) writes a whole tree of values, which
    // is what a test and a line of a log want.
    template<>
    struct formatter<value> {
        static constexpr bool takes(char) noexcept {
            return true;
        }

        static constexpr bool takes_precision() noexcept {
            return true;
        }

        static constexpr bool takes_nested(std::string_view) noexcept {
            return true;
        }

        static void write(format_sink& out, const value& v, const format_spec& spec,
                          std::string_view nested) {
            v.write(out, spec, nested);
        }
    };

    inline void value::write(format_sink& out, const format_spec& spec,
                             std::string_view nested) const {
        switch (kind()) {
            case value_kind::none:
                // Nothing, and not the word "none": a template writing a
                // name the data does not carry should leave a hole in
                // the page and not a word the reader has to wonder at.
                // The field is still filled, so a column stays a column.
                detail::write_as(out, std::string_view{}, spec, nested);
                return;
            case value_kind::boolean:
                detail::write_as(out, *get_if<bool>(&_held), spec, nested);
                return;
            case value_kind::integer: {
                long long n = *get_if<long long>(&_held);
                if (spec.type == 'c') {
                    // A code point here and not a byte, which is the one
                    // place a template parts company with a pattern and
                    // has to. {:c} of format.h narrows to a char, and
                    // over a number no char holds it throws — the value
                    // being what is wrong, not the pattern, and a
                    // pattern's caller is there to catch it. A template
                    // has no such caller: the number comes from the data
                    // and the letter from a file somebody edited, so the
                    // page would be lost to a pair of inputs that never
                    // met until it was rendered. And narrowing is wrong
                    // here besides — this module's text is UTF-8 and
                    // value(char) is refused for that very reason, so
                    // {:c} of 233 must be é and not the byte 0xE9, which
                    // is not a character at all.
                    //
                    // So the number is a code point, written as the
                    // bytes it takes, and a number that is no code point
                    // — negative, past U+10FFFF, or one of the surrogates
                    // — drops the letter and keeps the field, which is
                    // what every other specification a value will not
                    // take already does above.
                    if (n >= 0 && n <= 0x10FFFF && (n < 0xD800 || n > 0xDFFF)) {
                        detail::write_as(out, char32_t(n), spec, nested);
                        return;
                    }
                    format_spec plain = spec;
                    plain.type = 0;
                    detail::write_as(out, n, plain, nested);
                    return;
                }
                detail::write_as(out, n, spec, nested);
                return;
            }
            case value_kind::real:
                detail::write_as(out, *get_if<double>(&_held), spec, nested);
                return;
            case value_kind::text:
                detail::write_as(out, get_if<string>(&_held)->view(), spec, nested);
                return;
            case value_kind::list: {
                auto l = get_if<tracked_ptr<detail::value_list>>(&_held);
                if (*l) {
                    // A value may be put inside itself, and then this
                    // would not come back: the step counts the levels
                    // and the ellipsis is what stands where the walk
                    // was stopped
                    detail::value_step step;
                    if (!step) {
                        detail::write_as(out, std::string_view("..."), spec, nested);
                        return;
                    }
                    detail::write_as(out, (*l)->items, spec, nested);
                }
                return;
            }
            case value_kind::object: {
                auto o = get_if<tracked_ptr<detail::value_object>>(&_held);
                if (*o) {
                    detail::value_step step;
                    if (!step) {
                        detail::write_as(out, std::string_view("..."), spec, nested);
                        return;
                    }
                    detail::write_as(out, (*o)->fields, spec, nested);
                }
                return;
            }
        }
    }

    inline string value::to_string() const {
        if (auto s = get_if<string>(&_held)) {
            return *s;                          // the characters as they are, no quotes
        }
        if (is_none()) {
            return string();
        }
        char room[256];
        format_spec spec;
        format_sink out(room, sizeof room);
        write(out, spec);
        if (out.size() <= sizeof room) {
            return string(room, out.size());
        }
        std::string wider(out.size(), '\0');
        format_sink again(wider.data(), wider.size());
        write(again, spec);
        return string(wider.data(), again.size());
    }

    //--------------------------------------------------------------------
    // The functions every template has
    //--------------------------------------------------------------------
    namespace detail {
        // The five characters that change the meaning of HTML, and only
        // those. The apostrophe goes out as &#39; rather than &apos;,
        // which HTML 4 did not have and which some readers of old still
        // do not know.
        //
        // The runs that need nothing done to them go out whole rather
        // than a character at a time, which is the lesson the literal
        // runs of a format pattern already taught: most text has no
        // angle bracket in it at all and should cost a copy.
        inline string escape_html_text(const string& text) {
            auto v = text.view();
            size_t marked = 0;
            for (char c : v) {
                marked += (c == '&' || c == '<' || c == '>' || c == '"' || c == '\'') ? 1 : 0;
            }
            if (!marked) {
                return text;
            }
            std::string room;
            room.reserve(v.size() + marked * 5);
            size_t from = 0;
            for (size_t i = 0; i < v.size(); ++i) {
                const char* entity = nullptr;
                switch (v[i]) {
                    case '&':  entity = "&amp;"; break;
                    case '<':  entity = "&lt;"; break;
                    case '>':  entity = "&gt;"; break;
                    case '"':  entity = "&quot;"; break;
                    case '\'': entity = "&#39;"; break;
                    default:   break;
                }
                if (entity) {
                    room.append(v.data() + from, i - from);
                    room.append(entity);
                    from = i + 1;
                }
            }
            room.append(v.data() + from, v.size() - from);
            return string(room.data(), room.size());
        }
    }

    inline stencil_functions::stencil_functions() {
        // The case mappings are the full ones of case.h — "straße"
        // upper-cases to "STRASSE", and a letter whose mapping depends
        // on what stands around it is given what stands around it —
        // because a template has no business knowing a cheaper rule than
        // the module it lives in.
        add("upper", [](const value& v, slice<const value>) {
            return value(to_upper_full(v.to_string()));
        });
        add("lower", [](const value& v, slice<const value>) {
            return value(to_lower_full(v.to_string()));
        });
        add("title", [](const value& v, slice<const value>) {
            return value(to_title(v.to_string()));
        });
        add("trim", [](const value& v, slice<const value>) {
            return value(v.to_string().trim());
        });
        add("escape_html", [](const value& v, slice<const value>) {
            return value(detail::escape_html_text(v.to_string()));
        });
        // What to write where there is nothing to write. Empty and not
        // merely absent, as it is in Go: a name holding nought or no
        // characters at all takes the default too, which is what makes
        // {{ count | default "none" }} read the way it looks.
        add("default", [](const value& v, slice<const value> args) {
            if (v.truthy() || args.empty()) {
                return v;
            }
            return args[0];
        });
    }

    inline const stencil_functions& stencil_functions::builtin() {
        // Managed, and held by a root that may live in a global: the
        // table holds tracked pointers — every function is one — and a
        // tracked pointer must live on the stack or inside a managed
        // object, which a function-local static is neither. root_ptr is
        // what the library has for exactly this, and the table is made
        // once for the whole program.
        static root_ptr<stencil_functions> table = make_tracked<stencil_functions>();
        return *table;
    }

    //--------------------------------------------------------------------
    // Reading a source into steps.
    //
    // One pass, left to right, with a stack of the blocks left open. A
    // block writes down the step that has to be told where to jump and
    // is told when the block closes, which is the usual way a one-pass
    // compiler handles a forward jump and needs nothing read twice.
    //
    // Everything that can be settled here is settled here: the shape of
    // every specification, that every name of a function is one the
    // table knows, that every block is closed, that no jump goes
    // nowhere. What is left for the render is only what depends on the
    // values, which do not exist yet.
    //--------------------------------------------------------------------
    struct stencil_parser {
        const string& src;
        std::string_view text;
        const stencil_functions& functions;
        stencil out;
        stencil_error why = stencil_error(0, 1, 1, "");

        // The block being read, and the fixing up it is owed
        struct open_block {
            enum class kind : uint8_t { branch, loop, with };

            kind what = kind::branch;
            size_t at = 0;                  // where in the source it was opened
            size_t cond = size_t(-1);       // the step to be told where to go when its arm ends
            size_t body = 0;                // where the body of a loop starts
            bool had_else = false;
            vector<size_t> to_end;          // the jumps waiting for the end
        };

        vector<open_block> open{};   // braced, so that the aggregate below names four fields and not seven
        size_t depth = 0;

        bool fail(size_t at, const char* reason) {
            size_t line = 1;
            size_t column = 1;
            for (size_t i = 0; i < at && i < text.size(); ++i) {
                if (text[i] == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
            }
            why = stencil_error(at, line, column, reason);
            return false;
        }

        static bool is_space(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
        }

        // A name of the template's own language: what may stand as one
        // step of a path or as the name of a function. Deliberately
        // narrow — letters, digits and an underscore — because a name
        // that may hold anything cannot be told from the syntax around
        // it, and a mapping whose keys are sentences is reached through
        // the data and not through a path.
        static bool is_name_char(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_';
        }

        void emit(detail::stencil_op op, uint32_t a, uint32_t b) {
            detail::stencil_step s;
            s.op = op;
            s.a = a;
            s.b = b;
            out._steps.push_back(s);
        }

        size_t here() const {
            return out._steps.size();
        }

        //----------------------------------------------------------------
        // The pieces of an expression
        //----------------------------------------------------------------
        // A run of text in quotes, with the escapes a person writing a
        // template would expect. The characters are copied out, since
        // an escape means the value is not a run of the source.
        bool read_quoted(size_t& at, size_t end, string& made) {
            char quote = text[at];
            ++at;
            std::string room;
            while (at < end) {
                char c = text[at];
                if (c == quote) {
                    ++at;
                    made = string(room.data(), room.size());
                    return true;
                }
                if (c == '\\' && at + 1 < end) {
                    char e = text[at + 1];
                    switch (e) {
                        case 'n':  room.push_back('\n'); break;
                        case 't':  room.push_back('\t'); break;
                        case 'r':  room.push_back('\r'); break;
                        case '\\': room.push_back('\\'); break;
                        case '"':  room.push_back('"'); break;
                        case '\'': room.push_back('\''); break;
                        default:   return fail(at, "an escape nobody knows");
                    }
                    at += 2;
                    continue;
                }
                room.push_back(c);
                ++at;
            }
            return fail(at, "the text is not closed");
        }

        // One operand: a literal, or a path from the dot or from the
        // root. What is not one of those is refused here rather than
        // read as a name and found missing at render, when whoever
        // could fix it is no longer looking.
        bool read_operand(size_t& at, size_t end, detail::stencil_expr& e) {
            while (at < end && is_space(text[at])) {
                ++at;
            }
            if (at >= end) {
                return fail(at, "a value was expected");
            }
            char c = text[at];
            if (c == '"' || c == '\'') {
                string made;
                if (!read_quoted(at, end, made)) {
                    return false;
                }
                e.from = detail::stencil_expr::origin::literal;
                e.lit = uint32_t(out._literals.size());
                out._literals.push_back(value(made));
                return true;
            }
            if (c == '-' || (c >= '0' && c <= '9')) {
                size_t from = at;
                if (text[at] == '-') {
                    ++at;
                }
                bool real = false;
                while (at < end && ((text[at] >= '0' && text[at] <= '9') || text[at] == '.'
                                    || text[at] == 'e' || text[at] == 'E'
                                    || ((text[at] == '-' || text[at] == '+') && at > from
                                        && (text[at - 1] == 'e' || text[at - 1] == 'E')))) {
                    real = real || text[at] == '.' || text[at] == 'e' || text[at] == 'E';
                    ++at;
                }
                auto number = text.substr(from, at - from);
                value made;
                if (real) {
                    double d = 0;
                    auto r = std::from_chars(number.data(), number.data() + number.size(), d);
                    if (r.ec != std::errc() || r.ptr != number.data() + number.size()) {
                        return fail(from, "that is not a number");
                    }
                    made = value(d);
                } else {
                    long long v = 0;
                    auto r = std::from_chars(number.data(), number.data() + number.size(), v);
                    if (r.ec != std::errc() || r.ptr != number.data() + number.size()) {
                        return fail(from, "that is not a number");
                    }
                    made = value(v);
                }
                e.from = detail::stencil_expr::origin::literal;
                e.lit = uint32_t(out._literals.size());
                out._literals.push_back(made);
                return true;
            }
            // A path. `$` starts at the root of the data whatever block
            // it stands in, which is the only way out of a range; a
            // leading dot is the element being walked, and so is a name
            // with no dot before it — the shorthand this has that Go
            // does not, where a bare name is a function call.
            if (c == '$') {
                e.from = detail::stencil_expr::origin::root;
                ++at;
            } else {
                e.from = detail::stencil_expr::origin::dot;
                if (c == '.') {
                    // `.` alone is the element itself
                    if (at + 1 >= end || !is_name_char(text[at + 1])) {
                        ++at;
                        e.path_at = uint32_t(out._names.size());
                        e.path_size = 0;
                        return true;
                    }
                }
            }
            e.path_at = uint32_t(out._names.size());
            e.path_size = 0;
            bool first = true;
            while (at < end) {
                if (text[at] == '.') {
                    ++at;
                } else if (!first) {
                    break;
                }
                if (at >= end || !is_name_char(text[at])) {
                    if (first && e.from == detail::stencil_expr::origin::root) {
                        return true;            // `$` alone is the whole of the data
                    }
                    return fail(at, "a name was expected after the dot");
                }
                size_t from = at;
                while (at < end && is_name_char(text[at])) {
                    ++at;
                }
                out._names.push_back(string(text.data() + from, at - from));
                ++e.path_size;
                first = false;
                if (at >= end || text[at] != '.') {
                    break;
                }
            }
            if (!e.path_size && e.from == detail::stencil_expr::origin::dot) {
                return fail(at, "a value was expected");
            }
            return true;
        }

        // A whole field: an operand, the functions it is piped through,
        // and — where one is allowed — the specification after the
        // colon, which is read by format.h's own reader and by nothing
        // written here.
        bool read_expr(size_t from, size_t to, uint32_t& made, bool spec_allowed) {
            detail::stencil_expr e;
            size_t body_end = to;
            if (spec_allowed) {
                // The first colon standing outside quotes ends the
                // expression: neither a path nor the name of a function
                // may hold one, and a literal that does is in quotes,
                // which this walks past.
                char quote = 0;
                for (size_t i = from; i < to; ++i) {
                    char c = text[i];
                    if (quote) {
                        if (c == '\\') {
                            ++i;
                        } else if (c == quote) {
                            quote = 0;
                        }
                        continue;
                    }
                    if (c == '"' || c == '\'') {
                        quote = c;
                        continue;
                    }
                    if (c == ':') {
                        body_end = i;
                        break;
                    }
                }
            }
            size_t at = from;
            if (!read_operand(at, body_end, e)) {
                return false;
            }
            e.pipe_at = uint32_t(out._stages.size());
            e.pipe_size = 0;
            for (;;) {
                while (at < body_end && is_space(text[at])) {
                    ++at;
                }
                if (at >= body_end) {
                    break;
                }
                if (text[at] != '|') {
                    return fail(at, "a bar was expected between a value and a function");
                }
                ++at;
                while (at < body_end && is_space(text[at])) {
                    ++at;
                }
                size_t name_from = at;
                while (at < body_end && is_name_char(text[at])) {
                    ++at;
                }
                if (at == name_from) {
                    return fail(at, "the name of a function was expected");
                }
                string name(text.data() + name_from, at - name_from);
                auto fn = functions.find(name);
                if (!fn) {
                    // Found here and not at render, so that a template
                    // calling a function nobody wrote is a thing the
                    // program learns when it reads the file
                    return fail(name_from, "no function of that name");
                }
                detail::stencil_stage stage;
                stage.fn = uint32_t(out._functions.size());
                out._functions.push_back(*fn);
                stage.args_at = uint32_t(out._args.size());
                stage.args_size = 0;
                for (;;) {
                    while (at < body_end && is_space(text[at])) {
                        ++at;
                    }
                    if (at >= body_end || text[at] == '|') {
                        break;
                    }
                    if (stage.args_size == detail::MaxArgs) {
                        return fail(at, "too many arguments to one function");
                    }
                    detail::stencil_expr arg;
                    if (!read_operand(at, body_end, arg)) {
                        return false;
                    }
                    out._args.push_back(arg);
                    ++stage.args_size;
                }
                out._stages.push_back(stage);
                ++e.pipe_size;
            }
            if (body_end < to) {
                // Everything after the colon is format.h's to read, and
                // it is read by format.h: the same reader, the same
                // grammar, the same refusals. A nested specification is
                // opened only for a value that holds other values, and
                // a value here may hold them, so a second colon is
                // always offered.
                std::string_view body = text.substr(body_end + 1, to - body_end - 1);
                std::string_view nested;
                if (!detail::read_spec(body, e.spec, nested, true)) {
                    return fail(body_end + 1, "that is not a specification");
                }
                if (nested.data()) {
                    e.has_nested = true;
                    e.nested_at = uint32_t(nested.data() - text.data());
                    e.nested_size = uint32_t(nested.size());
                }
            }
            made = uint32_t(out._exprs.size());
            out._exprs.push_back(e);
            return true;
        }

        //----------------------------------------------------------------
        // The actions
        //----------------------------------------------------------------
        // A keyword, and not merely a name that starts with its
        // letters. The end of the action bounds it rather than the end
        // of the source, which is the whole of the difference between
        // {{end}} being read as the keyword and being read as a path
        // called "end": what follows it is a brace, not a space, and
        // only `to` knows that the action stops there.
        bool word_at(size_t at, size_t to, std::string_view word) const {
            if (to - at < word.size()) {
                return false;
            }
            if (text.compare(at, word.size(), word) != 0) {
                return false;
            }
            size_t after = at + word.size();
            return after == to || is_space(text[after]);
        }

        bool action(size_t from, size_t to) {
            while (from < to && is_space(text[from])) {
                ++from;
            }
            while (to > from && is_space(text[to - 1])) {
                --to;
            }
            if (from == to) {
                return fail(from, "an empty action");
            }
            auto starts = [&](std::string_view word) {
                return word_at(from, to, word);
            };
            if (starts("if")) {
                uint32_t e = 0;
                if (!read_expr(from + 2, to, e, false)) {
                    return false;
                }
                open_block b;
                b.what = open_block::kind::branch;
                b.at = from;
                emit(detail::stencil_op::branch, e, 0);
                b.cond = here() - 1;
                open.push_back(std::move(b));
                return true;
            }
            if (starts("range")) {
                uint32_t e = 0;
                if (!read_expr(from + 5, to, e, false)) {
                    return false;
                }
                open_block b;
                b.what = open_block::kind::loop;
                b.at = from;
                emit(detail::stencil_op::loop, e, 0);
                b.cond = here() - 1;
                b.body = here();
                open.push_back(std::move(b));
                depth = std::max(depth, open.size());
                return true;
            }
            if (starts("with")) {
                uint32_t e = 0;
                if (!read_expr(from + 4, to, e, false)) {
                    return false;
                }
                open_block b;
                b.what = open_block::kind::with;
                b.at = from;
                emit(detail::stencil_op::enter, e, 0);
                b.cond = here() - 1;
                open.push_back(std::move(b));
                depth = std::max(depth, open.size());
                return true;
            }
            if (starts("else")) {
                if (open.empty()) {
                    return fail(from, "an else with nothing open");
                }
                auto& b = open.back();
                if (b.had_else && b.cond == size_t(-1)) {
                    return fail(from, "a second else");
                }
                size_t rest = from + 4;
                while (rest < to && is_space(text[rest])) {
                    ++rest;
                }
                bool chained = rest < to;
                if (chained && b.what != open_block::kind::branch) {
                    return fail(rest, "only an if may be chained with else if");
                }
                if (chained && !word_at(rest, to, "if")) {
                    return fail(rest, "an if was expected after else");
                }
                // The arm that just ended jumps over everything left,
                // and where the condition failed is here
                if (b.what == open_block::kind::loop) {
                    emit(detail::stencil_op::repeat, 0, uint32_t(b.body));
                } else if (b.what == open_block::kind::with) {
                    emit(detail::stencil_op::leave, 0, 0);
                }
                emit(detail::stencil_op::jump, 0, 0);
                b.to_end.push_back(here() - 1);
                if (b.cond != size_t(-1)) {
                    out._steps[b.cond].b = uint32_t(here());
                }
                b.had_else = true;
                b.cond = size_t(-1);
                if (chained) {
                    // An else if opens no block of its own: the one
                    // end that closes the chain closes all of it, so
                    // the condition of this arm simply becomes the one
                    // the block is waiting to fix up
                    uint32_t e = 0;
                    if (!read_expr(rest + 2, to, e, false)) {
                        return false;
                    }
                    emit(detail::stencil_op::branch, e, 0);
                    b.cond = here() - 1;
                }
                return true;
            }
            if (starts("end")) {
                if (open.empty()) {
                    return fail(from, "an end with nothing open");
                }
                auto b = std::move(open.back());
                open.pop_back();
                if (!b.had_else) {
                    if (b.what == open_block::kind::loop) {
                        emit(detail::stencil_op::repeat, 0, uint32_t(b.body));
                    } else if (b.what == open_block::kind::with) {
                        emit(detail::stencil_op::leave, 0, 0);
                    }
                }
                if (b.cond != size_t(-1)) {
                    out._steps[b.cond].b = uint32_t(here());
                }
                for (auto j : b.to_end) {
                    out._steps[j].b = uint32_t(here());
                }
                return true;
            }
            uint32_t e = 0;
            if (!read_expr(from, to, e, true)) {
                return false;
            }
            emit(detail::stencil_op::write, e, 0);
            return true;
        }

        //----------------------------------------------------------------
        // The whole source
        //----------------------------------------------------------------
        bool run() {
            size_t n = text.size();
            size_t i = 0;
            bool trim_next = false;
            while (i < n) {
                size_t open_at = text.find("{{", i);
                size_t run_from = i;
                size_t run_to = open_at == std::string_view::npos ? n : open_at;
                if (trim_next) {
                    while (run_from < run_to && is_space(text[run_from])) {
                        ++run_from;
                    }
                    trim_next = false;
                }
                // A space after the minus is required, as it is in Go,
                // and for the same reason: without it {{-5}} is a trim
                // and the number five, where every reader of it sees
                // minus five.
                bool trims_before = open_at != std::string_view::npos
                                 && open_at + 3 < n && text[open_at + 2] == '-'
                                 && is_space(text[open_at + 3]);
                if (trims_before) {
                    while (run_to > run_from && is_space(text[run_to - 1])) {
                        --run_to;
                    }
                }
                if (run_to > run_from) {
                    emit(detail::stencil_op::text, uint32_t(run_from), uint32_t(run_to - run_from));
                }
                if (open_at == std::string_view::npos) {
                    break;
                }
                size_t body = open_at + 2 + (trims_before ? 1 : 0);
                // A comment is the whole of its action, so it is read
                // whole: nothing inside it is looked at and a closing
                // brace in it means nothing
                size_t lead = body;
                while (lead < n && is_space(text[lead])) {
                    ++lead;
                }
                if (lead + 1 < n && text[lead] == '/' && text[lead + 1] == '*') {
                    size_t shut = text.find("*/", lead + 2);
                    if (shut == std::string_view::npos) {
                        return fail(lead, "the comment is not closed");
                    }
                    size_t after = shut + 2;
                    while (after < n && is_space(text[after])) {
                        ++after;
                    }
                    if (after < n && text[after] == '-') {
                        trim_next = true;
                        ++after;
                    }
                    if (after + 1 >= n || text[after] != '}' || text[after + 1] != '}') {
                        return fail(after, "the comment does not end the action");
                    }
                    i = after + 2;
                    continue;
                }
                // The end of the action, with what stands in quotes
                // walked past so that a brace written inside a literal
                // does not close it
                size_t at = body;
                char quote = 0;
                size_t shut = std::string_view::npos;
                while (at + 1 < n) {
                    char c = text[at];
                    if (quote) {
                        if (c == '\\') {
                            ++at;
                        } else if (c == quote) {
                            quote = 0;
                        }
                        ++at;
                        continue;
                    }
                    if (c == '"' || c == '\'') {
                        quote = c;
                        ++at;
                        continue;
                    }
                    if (c == '}' && text[at + 1] == '}') {
                        shut = at;
                        break;
                    }
                    ++at;
                }
                if (shut == std::string_view::npos) {
                    return fail(open_at, "the action is not closed");
                }
                size_t body_to = shut;
                if (body_to > body && text[body_to - 1] == '-'
                    && body_to - 1 > body && is_space(text[body_to - 2])) {
                    // A minus glued to what stands before it is a minus
                    // sign; one with a space in front of it is the trim
                    trim_next = true;
                    --body_to;
                }
                if (!action(body, body_to)) {
                    return false;
                }
                i = shut + 2;
            }
            if (!open.empty()) {
                return fail(open.back().at, "a block was left open");
            }
            return true;
        }
    };

    inline expected<stencil, stencil_error> stencil::parse(const string& source,
                                                           const stencil_functions& functions) {
        stencil_parser parser{source, source.view(), functions, stencil()};
        parser.out._source = source;
        if (!parser.run()) {
            return unexpected(parser.why);
        }
        parser.out._depth = uint32_t(parser.depth);
        return std::move(parser.out);
    }

    //--------------------------------------------------------------------
    // Walking the steps.
    //
    // A program counter over the steps, a stack of dots — what `.` means
    // at this point — and a stack of frames for the loops that are open.
    // All three are sized before the walk starts from the depth the
    // source came to, so nothing they hold a pointer into moves and a
    // render allocates for its own bookkeeping not at all: what it
    // allocates is the page it hands back, and the values a function of
    // the pipeline makes.
    //--------------------------------------------------------------------
    namespace detail {
        struct value_reach {
            static const value_list* list_of(const value& v) noexcept {
                auto l = get_if<tracked_ptr<value_list>>(&v._held);
                return l ? l->get() : nullptr;
            }

            static const value_object* object_of(const value& v) noexcept {
                auto o = get_if<tracked_ptr<value_object>>(&v._held);
                return o ? o->get() : nullptr;
            }

            static value_object* object_of(value& v) noexcept {
                auto o = get_if<tracked_ptr<value_object>>(&v._held);
                return o ? o->get() : nullptr;
            }
        };
    }

    inline const value* stencil::_resolve(const detail::stencil_expr& e, const value* dot,
                                                const value* root) const noexcept {
        if (e.from == detail::stencil_expr::origin::literal) {
            return &_literals[e.lit];
        }
        const value* v = e.from == detail::stencil_expr::origin::root ? root : dot;
        for (uint32_t i = 0; i < e.path_size; ++i) {
            if (!v) {
                return nullptr;             // a name the data does not carry
            }
            v = v->find(_names[e.path_at + i]);
        }
        return v;
    }

    inline value stencil::_eval(const detail::stencil_expr& e, const value* dot,
                                      const value* root) const {
        const value* base = _resolve(e, dot, root);
        value held = base ? *base : value();
        for (uint32_t s = 0; s < e.pipe_size; ++s) {
            const auto& stage = _stages[e.pipe_at + s];
            // A stage that was written no arguments is given none, and
            // the room below is never made. Five of the six functions
            // every template has take none — upper, lower, title, trim,
            // escape_html — so this is the road and not the corner.
            //
            // What it saves is not the loop, which runs nought times
            // either way. It is that a value holds a variant with a
            // string in it, so eight of them are eight constructions
            // and eight destructions, and the compiler cannot take them
            // out: the room goes by pointer into a call it knows
            // nothing about, and after that call it cannot tell which
            // alternative any of them holds. Measured over ten rows of
            // {{ who | upper }}, both binaries in one directory under
            // names of the same length and an empty environment, best
            // of three alternated: 810.7 ns against 764.9, and with two
            // stages 1188.2 against 1066.8 — about five and a half
            // nanoseconds a stage. The same field with an argument,
            // where the room is still made, did not move.
            if (!stage.args_size) {
                held = _functions[stage.fn](held, slice<const value>());
                continue;
            }
            // On the stack and not in the allocator: a field is allowed
            // MaxArgs of them and the parser refused the source that
            // wrote more
            value room[detail::MaxArgs];
            for (uint32_t a = 0; a < stage.args_size; ++a) {
                const value* av = _resolve(_args[stage.args_at + a], dot, root);
                if (av) {
                    room[a] = *av;
                }
            }
            held = _functions[stage.fn](held, slice<const value>(room, size_t(stage.args_size)));
        }
        return held;
    }

    // Where the walk keeps what it is standing on. All three are sized
    // before it starts — a loop keeps the thing it walks, and a walk over
    // a mapping keeps the row it makes as well, so two for every block
    // that can open one — and nothing they hold a pointer into can move
    // after that.
    //
    // On the stack while the blocks are shallow, which they almost always
    // are. Three vectors would be three managed allocations for the
    // bookkeeping of a page that may have no block in it at all, and that
    // was most of what a render cost: a page of literal text with no
    // action in it read 74 ns, of which about fifty were these.
    //
    // And the deep road is a call of its own rather than a branch with
    // the vectors declared beside the arrays. Merely naming them there
    // builds and unbuilds three managed objects on the frame of every
    // render, the shallow ones included, which is what the last round of
    // this left behind: an empty page 17.6 ns against 13.8 and three
    // fields 143.8 against 138.9, for a road nearly no page takes.
    inline void stencil::_run(growing_sink& room, const value& data) const {
        if (_depth > InlineDepth) [[unlikely]] {
            _run_deep(room, data);
            return;
        }
        const value* dots[InlineDepth + 2];
        detail::stencil_frame frames[InlineDepth + 1];
        value owned[2 * InlineDepth + 2];
        _walk(room, data, dots, frames, owned);
    }

    // Past InlineDepth the vectors come back, because a template nested
    // that deep is doing enough work that three allocations do not show
    inline void stencil::_run_deep(growing_sink& room, const value& data) const {
        vector<const value*> dots;
        vector<detail::stencil_frame> frames;
        vector<value> owned;
        dots.resize(_depth + 2);
        frames.resize(_depth + 1);
        owned.resize(2 * _depth + 2);
        _walk(room, data, dots.data(), frames.data(), owned.data());
    }

    inline void stencil::_walk(growing_sink& room, const value& data, const value** dots,
                               detail::stencil_frame* frames, value* owned) const {
        // Where the page goes. The reference holds over a growth — the
        // sink is `room`'s own and is moved over the new characters
        // rather than made afresh — so the walk below never has to think
        // about it except where it writes.
        format_sink& out = room.out();

        // How much room there is, kept here and not asked of `room` at
        // every step. A local the compiler can prove nobody else reaches
        // sits in a register for the whole walk; a field of `room` has to
        // be read back after every call a step makes, because the call
        // might have written it. It is written back into only where the
        // room is added to, which is the one place that changes it.
        size_t cap = room.capacity();

        using frame = detail::stencil_frame;

        const value* root = &data;
        const value nothing;

        size_t n_dots = 0;
        size_t n_frames = 0;
        size_t n_owned = 0;
        dots[n_dots++] = root;

        // The row a walk over a mapping stands on: the name under `key`
        // and what it holds under `value`, which is what lets a template
        // read both without a syntax for naming variables. The two
        // fields are written over for every row rather than made afresh,
        // so the mapping is built once for the whole loop.
        auto set_row = [](value& row, const string& key, const value& held) {
            auto o = detail::value_reach::object_of(row);
            o->fields.insert_or_assign(string("key"), value(key));
            o->fields.insert_or_assign(string("value"), held);
        };

        size_t pc = 0;
        while (pc < _steps.size()) {
            const auto& s = _steps[pc];
            switch (s.op) {
                case detail::stencil_op::text:
                    // Whole, as a literal run of a format pattern is:
                    // most of a page is this step and a byte at a time
                    // would be the whole of what a render costs.
                    //
                    // The asking comes after the writing, as it does for
                    // a value, although this is the one step that knows
                    // its length in advance and could have asked before.
                    // Both were written and alternated and they read the
                    // same, so what is here is the one that asks nothing
                    // of format_sink: the size put has just written,
                    // against a capacity that never leaves its register.
                    //
                    // The hint is not decoration. Without it the
                    // compiler lays the growth out in line and jumps
                    // over it, so the page that fits takes a branch at
                    // every literal run of every row, and that — not the
                    // compare, which the disassembly puts at two
                    // instructions — was the fourteen per cent a page of
                    // ten rows had been paying.
                    out.put(_source.data() + s.a, s.b);
                    if (out.size() > cap) [[unlikely]] {
                        // Where this run began: size counts what did not
                        // fit as well as what did, so subtracting the run
                        // gives back the mark without having read one
                        cap = room.take_room(out.size(), out.size() - s.b);
                        break;      // more room now; this step goes again
                    }
                    ++pc;
                    break;
                case detail::stencil_op::write: {
                    size_t mark = out.size();
                    const auto& e = _exprs[s.a];
                    std::string_view nested;
                    if (e.has_nested) {
                        nested = std::string_view(_source.data() + e.nested_at, e.nested_size);
                    }
                    if (!e.pipe_size) {
                        // Nothing is made and nothing is copied: the
                        // value is written where it lies
                        const value* v = _resolve(e, dots[n_dots - 1], root);
                        (v ? *v : nothing).write(out, e.spec, nested);
                    } else {
                        _eval(e, dots[n_dots - 1], root).write(out, e.spec, nested);
                    }
                    // How long a value is, is not a thing anybody has
                    // until it is written, so this step finds out by
                    // writing and asks afterwards. The one step that did
                    // not fit is written again into room that now holds
                    // it: a path resolved once more and a pipeline run
                    // once more, which is why the contract on a
                    // pipeline's functions is that they are pure. The
                    // page before the mark stands where it was.
                    if (out.size() > cap) [[unlikely]] {
                        cap = room.take_room(out.size(), mark);
                        break;
                    }
                    ++pc;
                    break;
                }
                case detail::stencil_op::branch:
                    pc = _eval(_exprs[s.a], dots[n_dots - 1], root).truthy() ? pc + 1 : s.b;
                    break;
                case detail::stencil_op::jump:
                    pc = s.b;
                    break;
                case detail::stencil_op::enter: {
                    value held = _eval(_exprs[s.a], dots[n_dots - 1], root);
                    if (!held.truthy()) {
                        pc = s.b;
                        break;
                    }
                    owned[n_owned++] = std::move(held);
                    dots[n_dots++] = &owned[n_owned - 1];
                    ++pc;
                    break;
                }
                case detail::stencil_op::leave:
                    --n_dots;
                    owned[--n_owned] = value();
                    ++pc;
                    break;
                case detail::stencil_op::loop: {
                    size_t mark = n_owned;
                    // Kept, and not merely looked at: the elements are
                    // reached by pointer for the whole of the loop, and
                    // what holds them has to be alive that long
                    owned[n_owned++] = _eval(_exprs[s.a], dots[n_dots - 1], root);
                    const value& over = owned[n_owned - 1];
                    if (!over.size()) {
                        owned[--n_owned] = value();
                        pc = s.b;
                        break;
                    }
                    frame f;
                    f.mark = mark;
                    if (over.kind() == value_kind::list) {
                        // Straight at the elements: a walk over a list
                        // copies none of them
                        f.items = detail::value_reach::list_of(over);
                        f.index = 0;
                        dots[n_dots++] = &f.items->items[0];
                    } else {
                        f.fields = detail::value_reach::object_of(over);
                        f.it = f.fields->fields.begin();
                        owned[n_owned++] = object();
                        set_row(owned[n_owned - 1], f.it->first, f.it->second);
                        dots[n_dots++] = &owned[n_owned - 1];
                    }
                    frames[n_frames++] = f;
                    ++pc;
                    break;
                }
                case detail::stencil_op::repeat: {
                    frame& f = frames[n_frames - 1];
                    bool more = false;
                    if (f.items) {
                        ++f.index;
                        if (f.index < f.items->items.size()) {
                            dots[n_dots - 1] = &f.items->items[f.index];
                            more = true;
                        }
                    } else {
                        ++f.it;
                        if (f.it != f.fields->fields.end()) {
                            set_row(owned[n_owned - 1], f.it->first, f.it->second);
                            more = true;
                        }
                    }
                    if (more) {
                        pc = s.b;
                        break;
                    }
                    --n_dots;
                    while (n_owned > f.mark) {
                        owned[--n_owned] = value();
                    }
                    --n_frames;
                    ++pc;
                    break;
                }
            }
        }
    }

    inline size_t stencil::render_to(const slice<char>& buffer, const value& data) const {
        growing_sink out(growing_sink::lent, buffer.data(), buffer.size());
        _run(out, data);
        return out.size();
    }

    inline string stencil::render(const value& data) const {
        // The room a short page takes is on the stack, so a line of text
        // with two values in it is written once and allocates only for
        // the string it hands back.
        //
        // A longer one grows that room rather than being written twice,
        // which is where a page parts company with a format pattern. A
        // pattern's second pass is to_chars and memcpy and is cheaper
        // than the copying a doubling buffer does, and format() still
        // takes that road for exactly that reason. A page's second pass
        // is the whole walk again: every branch tested again, every row
        // of every loop again, every upper() and escape_html() again.
        // So the walk asks growing_sink after each step whether that
        // step ran off the end, and when it did it writes that one step
        // over into room twice the size — never the page.
        //
        // Measured over benchmarks/txt/stencil.cpp, which keeps both
        // roads so the comparison can be made again — its `sgcl` variant
        // against its `twopass` one, best of three alternated runs, a
        // page of rows with an if in each. A page of 1137 characters
        // 2355 ns against 4559, of 9879 characters 19.9 us against 40.6,
        // of 98709 characters 193.6 us against 401.9: half, at every
        // size, because the second walk was the whole of the cost and
        // the copying a doubling buffer does is next to none of it.
        //
        // The page that fits the stack room does ask, at every step that
        // writes, and what that asking costs is a round of its own. It
        // is not the compare, which is two instructions: it is that a
        // test whose other side calls is laid out with the common case
        // jumping over the call, so a page of ten rows took a branch at
        // every literal run and paid fourteen per cent for it. With the
        // test marked unlikely the page that fits falls straight
        // through, and against the commit before the room could grow at
        // all — two binaries alternated in one window, best of three —
        // every row is level: empty 28.9 against 28.7, simple 51.2
        // against 51.9, five 178.5 against 178.1, branch 57.1 against
        // 55.9, rows 521.8 against 526.2, render_to 502.5 against 506.8.
        // A hundred rows, where the page no longer fits, 5.1 us against
        // 10.1: the half stands and the rest of the range costs nothing.
        char room[1024];
        growing_sink out(room, sizeof room);
        _run(out, data);
        return out.text();
    }
}
