//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::yaml on any text. The first byte picks the options; the rest is
// the text. What must hold: every document read writes a text that reads back
// to an equal node and writes the same text again; its JSON and its hash are
// made without a fault; a text refused is refused at a place within it. Built
// with SGCL_FUZZ_LIBYAML (and libyaml linked: SGCL_FUZZ_LIBS="-DSGCL_FUZZ_LIBYAML
// -I/opt/homebrew/opt/libyaml/include /opt/homebrew/opt/libyaml/lib/libyaml.a"),
// a text both libyaml and this reading take reads to the same graph (the
// differences of YAML 1.1 and 1.2 that libyaml has passed over by name).
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/yaml_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef SGCL_FUZZ_LIBYAML
#include <yaml.h>
#include <map>
#include <vector>
#endif

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

#ifdef SGCL_FUZZ_LIBYAML
    const std::string core = "tag:yaml.org,2002:";

    std::string scalar(const char* tag, bool plain, const std::string& v) {
        std::string t = tag ? tag : "";
        char k = 's';
        std::string kept;
        if (t.empty() || (t != "!" && t.compare(0, core.size(), core) != 0)) {
            k = plain ? "nbifs"[int(sgcl::encoding::detail::yaml_resolve(v))] : 's';
            if (!t.empty()) {
                kept = "<" + t + ">";
            }
        } else if (t != "!") {
            std::string n = t.substr(core.size());
            k = n == "str" ? 's' : n == "int" ? 'i' : n == "float" ? 'f' : n == "bool" ? 'b' : n == "null" ? 'n' : 's';
            if (n != "str" && n != "int" && n != "float" && n != "bool" && n != "null") {
                k = plain ? "nbifs"[int(sgcl::encoding::detail::yaml_resolve(v))] : 's';
                kept = "<" + t + ">";
            }
        }
        return kept + k + "|" + v + "| ";
    }

    // libyaml's graph, "" when it refuses the text
    std::string libyaml(const std::string& text) {
        yaml_parser_t p;
        yaml_parser_initialize(&p);
        yaml_parser_set_input_string(&p, reinterpret_cast<const unsigned char*>(text.data()), text.size());
        std::string out;
        std::map<std::string, std::string> anchors;
        std::vector<std::pair<std::string, size_t>> open;
        bool ok = true;
        for (;;) {
            yaml_event_t e;
            if (!yaml_parser_parse(&p, &e)) {
                ok = false;
                break;
            }
            auto type = e.type;
            if (type == YAML_SCALAR_EVENT) {
                std::string d = scalar(reinterpret_cast<const char*>(e.data.scalar.tag), e.data.scalar.style == YAML_PLAIN_SCALAR_STYLE,
                                       std::string(reinterpret_cast<const char*>(e.data.scalar.value), e.data.scalar.length));
                if (e.data.scalar.anchor) {
                    anchors[reinterpret_cast<const char*>(e.data.scalar.anchor)] = d;
                }
                out += d;
            } else if (type == YAML_ALIAS_EVENT) {
                out += anchors[reinterpret_cast<const char*>(e.data.alias.anchor)];
            } else if (type == YAML_SEQUENCE_START_EVENT || type == YAML_MAPPING_START_EVENT) {
                bool seq = type == YAML_SEQUENCE_START_EVENT;
                const char* tag = reinterpret_cast<const char*>(seq ? e.data.sequence_start.tag : e.data.mapping_start.tag);
                const char* anchor = reinterpret_cast<const char*>(seq ? e.data.sequence_start.anchor : e.data.mapping_start.anchor);
                std::string t = tag ? tag : "";
                open.push_back({anchor ? anchor : "", out.size()});
                if (!t.empty() && t != "!" && t != core + (seq ? "seq" : "map")) {
                    out += "<" + t + ">";
                }
                out += seq ? "[ " : "{ ";
            } else if (type == YAML_SEQUENCE_END_EVENT || type == YAML_MAPPING_END_EVENT) {
                out += type == YAML_SEQUENCE_END_EVENT ? "] " : "} ";
                auto o = open.back();
                open.pop_back();
                if (!o.first.empty()) {
                    anchors[o.first] = out.substr(o.second);
                }
            } else if (type == YAML_DOCUMENT_END_EVENT) {
                out += "\n";
            }
            yaml_event_delete(&e);
            if (type == YAML_STREAM_END_EVENT || out.size() > 100000) {
                break;
            }
        }
        yaml_parser_delete(&p);
        return ok ? out : std::string();
    }

    void graph(const yaml& y, std::string& out) {
        std::string tag(y.tag().view());
        if (!tag.empty()) {
            out += "<" + tag + ">";
        }
        if (y.is_sequence()) {
            out += "[ ";
            for (auto& e : y.elements()) {
                graph(e, out);
            }
            out += "] ";
        } else if (y.is_mapping()) {
            out += "{ ";
            for (auto& m : y.members()) {
                graph(m.key, out);
                graph(m.value, out);
            }
            out += "} ";
        } else {
            out += "nbifs"[int(y.type())];
            out += "|" + std::string(y.text().view()) + "| ";
        }
    }
#endif
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 32768) {
        return 0;
    }
    yaml::options o;
    o.max_depth = (data[0] & 1) ? 8 : 128;
    o.max_alias_nodes = (data[0] & 2) ? 100 : 100000;
    o.allow_duplicate_keys = data[0] & 4;
    std::string text(reinterpret_cast<const char*>(data + 1), size - 1);
    auto docs = yaml::parse_all(string(text), o);
    if (!docs) {
        check(docs.error().offset() <= text.size());
        return 0;
    }
    yaml::options lax;
    lax.allow_duplicate_keys = o.allow_duplicate_keys;
    lax.max_alias_nodes = size_t(-1);
    for (const auto& d : *docs) {
        auto written = d.to_string();
        if (written.size() > 1000000) {
            continue;   // shared nodes written out at every place
        }
        auto back = yaml::parse(written, lax);
#ifdef SGCL_FUZZ_VERBOSE
        if (!back || !(*back == d)) {
            std::fprintf(stderr, "written:\n%s\n%s\n", written.data(), back ? "differs" : back.error().message().data());
        }
#endif
        check(back && *back == d && back->hash() == d.hash());
        check(back->to_string() == written);
        (void)d.to_json();
    }
#ifdef SGCL_FUZZ_LIBYAML
    // YAML 1.1's indicators that 1.2 reads as part of a plain scalar inside a
    // flow collection ("?a", "a:b"): libyaml takes them for a key and a value
    bool differs = false;
    for (size_t i = 0; i + 1 < text.size(); ++i) {
        char c = text[i], n = text[i + 1];
        differs = differs || ((c == '?' || c == ':') && n != ' ' && n != '\n' && n != '\r' && n != '\t');
        // 1.2's anchor names take any character but the blanks and the flow
        // indicators; libyaml's, letters, digits, '-' and '_'
        if (c == '&' || c == '*') {
            for (size_t k = i + 1; k < text.size(); ++k) {
                char a = text[k];
                if (a == ' ' || a == '\t' || a == '\n' || a == '\r' || a == ',' || a == '[' || a == ']' || a == '{' || a == '}') {
                    break;
                }
                if (!((a >= 'a' && a <= 'z') || (a >= 'A' && a <= 'Z') || (a >= '0' && a <= '9') || a == '-' || a == '_')) {
                    differs = true;
                }
            }
        }
    }
    if (!o.allow_duplicate_keys && o.max_depth > 8 && !differs) {
        std::string theirs = libyaml(text);
        if (!theirs.empty() && theirs.size() < 100000) {
            std::string ours;
            for (const auto& d : *docs) {
                graph(d, ours);
                ours += "\n";
            }
#ifdef SGCL_FUZZ_VERBOSE
            if (ours != theirs) {
                std::fprintf(stderr, "ours:\n%s\ntheirs:\n%s\n", ours.c_str(), theirs.c_str());
            }
#endif
            check(ours == theirs);
        }
    }
#endif
    return 0;
}
