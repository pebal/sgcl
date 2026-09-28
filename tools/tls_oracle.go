// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the TLS interoperability tests (tests/net/tls_interop.cpp):
// a crypto/tls server of Go's standard library, as an oracle only. It listens
// on 127.0.0.1 at a port of the system's choice and prints "LISTEN <port>",
// takes one connection, prints "STATE <cipher suite> <curve id> <alpn>" after
// the handshake, answers every line with the line reversed, and prints
// "CLOSE_NOTIFY" when the client's close_notify ends the stream (io.EOF),
// "ERROR <text>" for any other end. With -connect it is a client instead
// (see client below). Built by the test with `go build`.
package main

import (
	"bufio"
	"crypto/tls"
	"crypto/x509"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"strings"
)

// The client mode: connects, prints "STATE <cipher suite> <curve id> <alpn>",
// writes "hello go", prints "GOT <the line read back>", closes (close_notify)
func client(addr, ca, name, curves, alpn string) {
	pem, err := os.ReadFile(ca)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	roots := x509.NewCertPool()
	roots.AppendCertsFromPEM(pem)
	cfg := &tls.Config{RootCAs: roots, ServerName: name, MinVersion: tls.VersionTLS13}
	if curves != "" {
		for _, c := range strings.Split(curves, ",") {
			var id int
			fmt.Sscan(c, &id)
			cfg.CurvePreferences = append(cfg.CurvePreferences, tls.CurveID(id))
		}
	}
	if alpn != "" {
		cfg.NextProtos = strings.Split(alpn, ",")
	}
	c, err := tls.Dial("tcp", addr, cfg)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	st := c.ConnectionState()
	fmt.Printf("STATE %d %d %s\n", st.CipherSuite, st.CurveID, st.NegotiatedProtocol)
	c.Write([]byte("hello go\n"))
	line, err := bufio.NewReader(c).ReadString('\n')
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	fmt.Printf("GOT %s", line)
	c.Close()
	fmt.Println("CLOSED")
}

func main() {
	cert := flag.String("cert", "", "certificate chain (PEM)")
	key := flag.String("key", "", "private key (PEM)")
	curves := flag.String("curves", "", "comma-separated curve ids (decimal); empty: Go's default")
	alpn := flag.String("alpn", "", "comma-separated ALPN protocols")
	connect := flag.String("connect", "", "client mode: the address to connect to")
	ca := flag.String("ca", "", "client mode: the roots (PEM)")
	name := flag.String("servername", "localhost", "client mode: the server name")
	flag.Parse()
	if *connect != "" {
		client(*connect, *ca, *name, *curves, *alpn)
		return
	}
	pair, err := tls.LoadX509KeyPair(*cert, *key)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	cfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS13}
	if *curves != "" {
		for _, c := range strings.Split(*curves, ",") {
			var id int
			fmt.Sscan(c, &id)
			cfg.CurvePreferences = append(cfg.CurvePreferences, tls.CurveID(id))
		}
	}
	if *alpn != "" {
		cfg.NextProtos = strings.Split(*alpn, ",")
	}
	ln, err := tls.Listen("tcp", "127.0.0.1:0", cfg)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	fmt.Println("LISTEN", ln.Addr().(*net.TCPAddr).Port)
	c, err := ln.Accept()
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	tc := c.(*tls.Conn)
	if err := tc.Handshake(); err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	st := tc.ConnectionState()
	fmt.Printf("STATE %d %d %s\n", st.CipherSuite, st.CurveID, st.NegotiatedProtocol)
	r := bufio.NewReader(tc)
	for {
		line, err := r.ReadString('\n')
		if line != "" {
			b := []byte(strings.TrimSuffix(line, "\n"))
			for i, j := 0, len(b)-1; i < j; i, j = i+1, j-1 {
				b[i], b[j] = b[j], b[i]
			}
			tc.Write(append(b, '\n'))
		}
		if err != nil {
			if errors.Is(err, io.EOF) {
				fmt.Println("CLOSE_NOTIFY")
			} else {
				fmt.Println("ERROR", err)
			}
			return
		}
	}
}
