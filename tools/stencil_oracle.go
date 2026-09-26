// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for sgcl/txt/stencil.h: Go's own text/template asked the
// same questions, its answers written out as a C++ header the test
// includes. Run it from the root of the tree:
//
//     go run tools/stencil_oracle.go > tests/txt/stencil_tests.h
//
// Only the subset both sides really share is here, and where they differ
// the case is left out rather than bent until it passes:
//
//   - a bare name. {{ name }} here means the field of the dot, which is
//     what Jinja and Handlebars read it as; over there a name with no dot
//     is a function call. So every source below is written with the dot,
//     which both sides read the same way.
//   - a name the data does not carry. Go writes "<no value>", this
//     writes nothing, and that is a decision and not an accident.
//   - a walk over a mapping. Go's dot is the value and its order is the
//     keys sorted; here the dot is a row of key and value and the order
//     is the one the caller wrote.
//   - the full case mappings. upper of "strasse" with an eszett is
//     "STRASSE" here (case.h, one letter becoming two) and over there
//     the eszett is left alone, so the case functions are asked only
//     about ASCII here and the full mappings are checked by hand.
//   - escape_html. The placement is the same; the spelling of a quote is
//     &quot; here and &#34; there, so that one is checked by hand too.
//   - a specification after a colon, {{ .n:>8.2f }}, which Go has no
//     equivalent of at all: that is format.h's grammar and is checked
//     against format.h.
package main

import (
	"bytes"
	"fmt"
	"os"
	"strings"
	"text/template"
)

// The one shape of data every case below is rendered over, written the
// same way on both sides
func data() map[string]interface{} {
	return map[string]interface{}{
		"name":   "ada lovelace",
		"spaced": "  Countess  ",
		"admin":  true,
		"guest":  false,
		"n":      42,
		"zero":   0,
		"pi":     3.14159,
		"whole":  88.0,
		"empty":  "",
		"tags":   []interface{}{"one", "two", "three"},
		"nolist": []interface{}{},
		"user": map[string]interface{}{
			"first": "ada",
			"last":  "lovelace",
		},
		"rows": []interface{}{
			map[string]interface{}{"who": "ada", "on": true},
			map[string]interface{}{"who": "bob", "on": false},
		},
	}
}

// The same six names this header gives a template, written here to the
// same rule, so that a pipeline is asked the same question on both sides
func funcs() template.FuncMap {
	return template.FuncMap{
		"upper": strings.ToUpper,
		"lower": strings.ToLower,
		"title": strings.Title,
		"trim":  strings.TrimSpace,
		"default": func(fallback, in interface{}) interface{} {
			if truthy(in) {
				return in
			}
			return fallback
		},
	}
}

func truthy(v interface{}) bool {
	switch t := v.(type) {
	case nil:
		return false
	case bool:
		return t
	case int:
		return t != 0
	case float64:
		return t != 0
	case string:
		return t != ""
	case []interface{}:
		return len(t) != 0
	case map[string]interface{}:
		return len(t) != 0
	}
	return true
}

var sources = []string{
	// a path, plain and nested
	"hello {{.name}}",
	"{{.user.first}} {{.user.last}}",
	"{{.n}}",
	"{{.pi}}",
	"{{.whole}}",
	"{{.admin}}",
	"{{.guest}}",
	"{{.empty}}!",
	// text with no action in it at all
	"nothing happens here",
	"",
	// a truth asked of every shape a value comes in
	"{{if .admin}}yes{{else}}no{{end}}",
	"{{if .guest}}yes{{else}}no{{end}}",
	"{{if .zero}}yes{{else}}no{{end}}",
	"{{if .n}}yes{{else}}no{{end}}",
	"{{if .empty}}yes{{else}}no{{end}}",
	"{{if .name}}yes{{else}}no{{end}}",
	"{{if .tags}}some{{else}}none{{end}}",
	"{{if .nolist}}some{{else}}none{{end}}",
	"{{if .user}}some{{else}}none{{end}}",
	// an if with no else, and one chained
	"a{{if .admin}}B{{end}}c",
	"a{{if .guest}}B{{end}}c",
	"{{if .guest}}a{{else if .admin}}b{{else}}c{{end}}",
	"{{if .guest}}a{{else if .zero}}b{{else}}c{{end}}",
	"{{if .admin}}a{{else if .zero}}b{{else}}c{{end}}",
	// a walk over a list, the dot in it, and the root reached from it
	"{{range .tags}}[{{.}}]{{end}}",
	"{{range .tags}}{{.}}-{{$.n}} {{end}}",
	"{{range .nolist}}x{{end}}",
	"{{range .nolist}}x{{else}}empty{{end}}",
	"{{range .tags}}x{{else}}empty{{end}}",
	// one walk inside another, and an if inside a walk
	"{{range .rows}}{{.who}}:{{if .on}}on{{else}}off{{end}} {{end}}",
	"{{range .tags}}{{range $.tags}}.{{end}}|{{end}}",
	// with
	"{{with .user}}{{.first}} {{.last}}{{end}}",
	"{{with .empty}}yes{{else}}no{{end}}",
	"{{with .nolist}}yes{{else}}no{{end}}",
	"{{with .user}}{{.first}}{{end}}-{{.name}}",
	// a comment writes nothing
	"a{{/* nothing at all */}}b",
	"{{/* only a comment */}}",
	// the white space around an action, taken away on either side
	"a   {{- .n}}",
	"{{.n -}}   b",
	"a   {{- .n -}}   b",
	"line\n  {{- if .admin}}on{{end}}",
	// the pipeline
	"{{.name | upper}}",
	"{{.name | title}}",
	"{{.spaced | trim}}!",
	"{{.name | upper | lower}}",
	"{{.empty | default \"anon\"}}",
	"{{.name | default \"anon\"}}",
	"{{.zero | default 7}}",
	"{{.n | default 7}}",
	"{{range .tags}}{{. | upper}} {{end}}",
	// a doubled brace is one brace, on both sides through a literal
	"{{.n}}%",
}

func cppQuote(s string) string {
	var b strings.Builder
	b.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch c {
		case '"':
			b.WriteString("\\\"")
		case '\\':
			b.WriteString("\\\\")
		case '\n':
			b.WriteString("\\n")
		case '\t':
			b.WriteString("\\t")
		case '\r':
			b.WriteString("\\r")
		default:
			if c < 0x20 || c == 0x7F {
				b.WriteString(fmt.Sprintf("\\x%02x", c))
			} else {
				b.WriteByte(c)
			}
		}
	}
	b.WriteByte('"')
	return b.String()
}

func main() {
	type result struct{ source, want string }
	var out []result
	for _, s := range sources {
		t, err := template.New("t").Funcs(funcs()).Parse(s)
		if err != nil {
			fmt.Fprintf(os.Stderr, "go refused %q: %v\n", s, err)
			os.Exit(1)
		}
		var buf bytes.Buffer
		if err := t.Execute(&buf, data()); err != nil {
			fmt.Fprintf(os.Stderr, "go failed on %q: %v\n", s, err)
			os.Exit(1)
		}
		out = append(out, result{s, buf.String()})
	}

	fmt.Println("//------------------------------------------------------------------------------")
	fmt.Println("// SGCL: a C++20 application framework")
	fmt.Println("// Copyright (c) 2022-2026 Sebastian Nibisz")
	fmt.Println("// SPDX-License-Identifier: Apache-2.0")
	fmt.Println("//------------------------------------------------------------------------------")
	fmt.Println("#pragma once")
	fmt.Println()
	fmt.Println("// Generated by tools/stencil_oracle.go: do not edit.")
	fmt.Println("//")
	fmt.Println("// What Go's text/template writes for the subset of the syntax both sides")
	fmt.Println("// share, over one shape of data that tests/txt/stencil.cpp builds the")
	fmt.Println("// same way. The source of that program says which cases are deliberately")
	fmt.Println("// not here and why.")
	fmt.Println()
	fmt.Println("namespace oracle {")
	fmt.Println("    struct TemplateCase {")
	fmt.Println("        const char* source;")
	fmt.Println("        const char* want;")
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    inline constexpr TemplateCase GoTemplateTests[] = {")
	for _, r := range out {
		fmt.Printf("        {%s, %s},\n", cppQuote(r.source), cppQuote(r.want))
	}
	fmt.Println("    };")
	fmt.Println("}")
}
