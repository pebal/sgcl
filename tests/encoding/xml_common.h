//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the XML tests share: a token as one line of text, in the form
// tools/xml_oracle.go prints Go's tokens in, so that the two can be
// compared as strings; a document read whole or through a stream cut into
// pieces; the files of the W3C conformance suite when they are on disk.
#pragma once

#include "common.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace xml_test {
    using sgcl::encoding::xml;
    namespace io = sgcl::io;

    // Bytes a line can show: printable ASCII as it is, the rest as \xNN
    inline std::string shown(std::string_view s) {
        static constexpr char Digits[] = "0123456789abcdef";
        std::string out = "\"";
        for (char c : s) {
            uint8_t b = uint8_t(c);
            if (b < 0x20 || b == '"' || b == '\\' || b == 0x7F) {
                out += "\\x";
                out += Digits[b >> 4];
                out += Digits[b & 15];
            } else {
                out += c;
            }
        }
        return out + "\"";
    }

    // A name as Go's xml.Name has it: {space}local, where the space of an
    // xmlns attribute is "xmlns" (xmlns:p) or nothing (xmlns itself)
    inline std::string go_name(std::string_view qname, std::string_view uri) {
        auto colon = qname.find(':');
        auto local = colon == std::string_view::npos ? qname : qname.substr(colon + 1);
        if (uri == "http://www.w3.org/2000/xmlns/") {
            if (qname == "xmlns") {
                return "{}xmlns";
            }
            return "{xmlns}" + std::string(local);
        }
        return "{" + std::string(uri) + "}" + std::string(local);
    }

    // One token as a line. Attribute values with their white space made
    // spaces: Go keeps a literal tab or line feed where XML 1.0 3.3.3 makes
    // it a space, and a line cannot tell a literal one from a reference
    // (the difference is named in the tests of values)
    inline std::string line(const xml::token& t, bool go_values = true) {
        using kind = xml::token::kind;
        switch (t.type()) {
            case kind::start_element: {
                std::string s = "S " + go_name(t.name().view(), t.namespace_uri().view());
                for (auto& a : t.attributes()) {
                    std::string v(a.value.view());
                    if (go_values) {
                        for (auto& c : v) {
                            c = (c == '\t' || c == '\n' || c == '\r') ? ' ' : c;
                        }
                    }
                    s += " " + go_name(a.name.view(), a.namespace_uri.view()) + "=" + shown(v);
                }
                return s;
            }
            case kind::end_element:
                return "E " + go_name(t.name().view(), t.namespace_uri().view());
            case kind::text:
                return "T " + shown(t.text().view());
            case kind::comment:
                return "C " + shown(t.text().view());
            case kind::instruction:
                return "P " + std::string(t.name().view()) + " " + shown(t.text().view());
            case kind::doctype:
                return "D " + std::string(t.name().view());
        }
        return "?";
    }

    struct Dump {
        std::string tokens;       // a line a token, text tokens that follow each other joined
        bool ok = false;
        sgcl::optional<sgcl::encoding::error> error;
    };

    // Text tokens next to each other are one line: how a reader splits a
    // text (around a CDATA section, at a comment it drops) is its own
    // business, and Go's and this reader's differ there
    inline void append(Dump& d, std::string& pending, const xml::token& t) {
        if (t.type() == xml::token::kind::text) {
            pending.append(t.text().data(), t.text().size());
            return;
        }
        if (!pending.empty()) {
            d.tokens += "T " + shown(pending) + "\n";
            pending.clear();
        }
        d.tokens += line(t) + "\n";
    }

    inline Dump dump(xml::reader& r) {
        Dump d;
        std::string pending;
        while (auto t = r.next()) {
            append(d, pending, *t);
        }
        if (!pending.empty()) {
            d.tokens += "T " + shown(pending) + "\n";
        }
        d.error = r.last_error();
        d.ok = !d.error;
        return d;
    }

    inline Dump dump(std::string_view doc, const xml::options& o = {}) {
        xml::reader r(sgcl::string(doc), o);
        return dump(r);
    }

    // The same document through a stream handing out `piece` bytes a read
    inline Dump dump_stream(std::string_view doc, size_t piece, const xml::options& o = {}) {
        xml::reader r(sgcl::make_tracked<enc_test::dribble>(std::string(doc), piece), o);
        return dump(r);
    }

    // Where the W3C XML Conformance Test Suite lies (tools, not the
    // repository: ~/Programming/oracles/xmlconf), or "" when it is not there
    inline std::string xmlconf_root() {
        const char* home = std::getenv("HOME");
        if (!home) {
            return {};
        }
        std::string root = std::string(home) + "/Programming/oracles/xmlconf/xmlconf/";
        std::ifstream probe(root + "xmlconf.xml");
        return probe ? root : std::string();
    }

    inline std::string read_file(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    // The tests of the W3C suite the XML tests take (xml_conformance.cpp
    // says which and why), read out of its catalogs with this module's
    // own reader
    struct Conformance {
        std::string id;
        std::string type;
        std::string entities;
        std::string file;
        std::string output;
    };

    const char* const Catalogs[] = {
        "xmltest/xmltest.xml",
        "japanese/japanese.xml",
        "sun/sun-valid.xml",
        "sun/sun-invalid.xml",
        "sun/sun-not-wf.xml",
        "oasis/oasis.xml",
        "ibm/ibm_oasis_invalid.xml",
        "ibm/ibm_oasis_not-wf.xml",
        "ibm/ibm_oasis_valid.xml",
        "eduni/errata-2e/errata2e.xml",
        "eduni/errata-3e/errata3e.xml",
        "eduni/errata-4e/errata4e.xml",
        "eduni/namespaces/1.0/rmt-ns10.xml",
        "eduni/namespaces/errata-1e/errata1e.xml",
        "eduni/misc/ht-bh.xml",
    };

    inline std::vector<Conformance> tests_of(const std::string& root) {
        std::vector<Conformance> out;
        for (auto catalog : Catalogs) {
            std::string path = root + catalog;
            std::string dir = path.substr(0, path.rfind('/') + 1);
            // a catalog of Sun's is an external entity of the main one: a
            // list of TEST elements with no root, wrapped here into one
            std::string text = read_file(path);
            if (text.starts_with("<?xml")) {
                text = text.substr(text.find("?>") + 2);
            }
            auto doc = xml::parse(string("<TESTCASES>" + text + "</TESTCASES>"));
            if (!doc) {
                ADD_FAILURE() << catalog << ": " << doc.error().message().view();
                continue;
            }
            // every TEST at any depth of TESTCASES (IBM's nest them)
            sgcl::vector<xml> found;   // nodes hold tracked pointers: managed vectors
            sgcl::vector<xml> groups{*doc};
            while (!groups.empty()) {
                xml g = groups.back();
                groups.pop_back();
                for (auto t : g.children("TEST")) {
                    found.push_back(t);
                }
                for (auto inner : g.children("TESTCASES")) {
                    groups.push_back(inner);
                }
            }
            for (auto& t : found) {
                auto get = [&](const char* n) { return std::string(t.attribute(n).value_or("").view()); };
                std::string version = get("VERSION");
                std::string recommendation = get("RECOMMENDATION");
                std::string edition = get("EDITION");
                if (version == "1.1" || recommendation == "XML1.1" || recommendation == "NS1.1") {
                    continue;
                }
                if (!edition.empty() && edition.find('5') == std::string::npos) {
                    continue;
                }
                if (get("NAMESPACE") == "no" || get("TYPE") == "error") {
                    continue;
                }
                Conformance c;
                c.id = get("ID");
                c.type = get("TYPE");
                c.entities = get("ENTITIES");
                c.file = dir + get("URI");
                if (!get("OUTPUT").empty()) {
                    c.output = dir + get("OUTPUT");
                }
                out.push_back(c);
            }
        }
        return out;
    }

}
