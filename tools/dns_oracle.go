// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the DNS interoperability test (tests/net/dns_interop.cpp):
// Go's own stub resolver (net.Resolver with PreferGo, its Dial pointed at
// the test's server on the loopback, over UDP or TCP as Go asks), as an
// oracle only. Each argument after -server is a query, "mx NAME", "txt
// NAME", "srv SERVICE PROTO NAME", "ns NAME" or "cname NAME"; for each, one
// line per record, "<query> | <record>", in Go's order:
//
//	mx example.test. | MX 10 mx1.example.test.
//	txt example.test. | TXT "v=spf1 -all"
//	srv sip tcp example.test. | SRV 10 60 5060 sip1.example.test.
//	ns example.test. | NS ns1.example.test.
//	cname www.example.test. | CNAME host.example.test.
//
// or "<query> | ERR notfound" (IsNotFound: NXDOMAIN, NODATA), "ERR timeout"
// or "ERR other". Built by the test with `go build`.
package main

import (
	"context"
	"flag"
	"fmt"
	"net"
	"os"
	"strings"
	"time"
)

func failure(err error) string {
	if e, ok := err.(*net.DNSError); ok {
		if e.IsNotFound {
			return "ERR notfound"
		}
		if e.IsTimeout {
			return "ERR timeout"
		}
	}
	return "ERR other"
}

func main() {
	server := flag.String("server", "", "the DNS server, host:port")
	flag.Parse()
	r := &net.Resolver{
		PreferGo: true,
		Dial: func(ctx context.Context, network, address string) (net.Conn, error) {
			var d net.Dialer
			return d.DialContext(ctx, network, *server)
		},
	}
	for _, q := range flag.Args() {
		f := strings.Fields(q)
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		var lines []string
		var err error
		switch f[0] {
		case "mx":
			var mx []*net.MX
			mx, err = r.LookupMX(ctx, f[1])
			for _, m := range mx {
				lines = append(lines, fmt.Sprintf("MX %d %s", m.Pref, m.Host))
			}
		case "txt":
			var txt []string
			txt, err = r.LookupTXT(ctx, f[1])
			for _, t := range txt {
				lines = append(lines, fmt.Sprintf("TXT %q", t))
			}
		case "srv":
			var srv []*net.SRV
			_, srv, err = r.LookupSRV(ctx, f[1], f[2], f[3])
			for _, s := range srv {
				lines = append(lines, fmt.Sprintf("SRV %d %d %d %s", s.Priority, s.Weight, s.Port, s.Target))
			}
		case "ns":
			var ns []*net.NS
			ns, err = r.LookupNS(ctx, f[1])
			for _, n := range ns {
				lines = append(lines, "NS "+n.Host)
			}
		case "cname":
			var c string
			c, err = r.LookupCNAME(ctx, f[1])
			if err == nil {
				lines = append(lines, "CNAME "+c)
			}
		default:
			fmt.Fprintln(os.Stderr, "unknown query", q)
			os.Exit(2)
		}
		cancel()
		if err != nil {
			lines = []string{failure(err)}
		}
		for _, l := range lines {
			fmt.Printf("%s | %s\n", q, l)
		}
	}
}
