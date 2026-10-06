// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the SVCB/HTTPS test (tests/net/dns_https.cpp, AgainstGo):
// golang.org/x/net/dns/dnsmessage reads DNS answers, as an oracle only.
// The package is the copy vendored in Go's standard library, which the test
// copies into a module of its own beside this file (package "dnsmessage"
// of module "svcboracle"): nothing is downloaded. Each line of input is a
// message in hex and the type asked (64 SVCB, 65 HTTPS); each line of output
// the records of the answer section in order, "PRIORITY TARGET K=HEX,K=HEX;"
// (the parameters' keys and values as they came), or "ERR" when the message
// does not read.
package main

import (
	"bufio"
	"encoding/hex"
	"fmt"
	"os"
	"strconv"
	"strings"

	"svcboracle/dnsmessage"
)

func read(m []byte, t dnsmessage.Type) string {
	var p dnsmessage.Parser
	if _, err := p.Start(m); err != nil {
		return "ERR"
	}
	if err := p.SkipAllQuestions(); err != nil {
		return "ERR"
	}
	var out strings.Builder
	for {
		h, err := p.AnswerHeader()
		if err == dnsmessage.ErrSectionDone {
			break
		}
		if err != nil {
			return "ERR"
		}
		if h.Type != t {
			if err := p.SkipAnswer(); err != nil {
				return "ERR"
			}
			continue
		}
		var r dnsmessage.SVCBResource
		if t == dnsmessage.TypeHTTPS {
			h, err := p.HTTPSResource()
			if err != nil {
				return "ERR"
			}
			r = h.SVCBResource
		} else {
			if r, err = p.SVCBResource(); err != nil {
				return "ERR"
			}
		}
		fmt.Fprintf(&out, "%d %s", r.Priority, r.Target.String())
		sep := " "
		for _, q := range r.Params {
			fmt.Fprintf(&out, "%s%d=%s", sep, uint16(q.Key), hex.EncodeToString(q.Value))
			sep = ","
		}
		out.WriteString(";")
	}
	return out.String()
}

func main() {
	in := bufio.NewScanner(os.Stdin)
	in.Buffer(make([]byte, 1<<20), 1<<20)
	w := bufio.NewWriter(os.Stdout)
	defer w.Flush()
	for in.Scan() {
		f := strings.Fields(in.Text())
		if len(f) != 2 {
			fmt.Fprintln(w, "ERR")
			continue
		}
		m, err := hex.DecodeString(f[0])
		t, err2 := strconv.Atoi(f[1])
		if err != nil || err2 != nil {
			fmt.Fprintln(w, "ERR")
			continue
		}
		fmt.Fprintln(w, read(m, dnsmessage.Type(t)))
	}
}
