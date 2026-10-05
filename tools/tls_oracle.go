// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The Go side of the TLS interoperability tests (tests/net/tls_interop.cpp):
// a crypto/tls server of Go's standard library, as an oracle only. It listens
// on 127.0.0.1 at a port of the system's choice and prints "LISTEN <port>",
// takes -n connections one after another (1 by default), prints "STATE
// <cipher suite> <curve id> <alpn> resumed=<bool> peer=<the client's
// common name, or ->" after each handshake, answers every line with the
// line reversed, and prints "CLOSE_NOTIFY" when the client's close_notify
// ends the stream (io.EOF), "ERROR <text>" for any other end. Session
// tickets are on (Go's default); -clientauth request|require asks for a
// client certificate (verified against -clientca when given, Go's
// VerifyClientCertIfGiven and RequireAndVerifyClientCert). With -connect it
// is a client instead (see client below). -tls12 limits either side to TLS
// 1.2 (MinVersion and MaxVersion 1.2; -ciphers then names the suites by
// their decimal ids, Go's default otherwise), and
// STATE ends with "version=<hex>". Built by the test with `go build`.
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

// The client mode: -n connections one after another, sharing a session
// cache (resumption), each: connects, prints "STATE <cipher suite> <curve
// id> <alpn> resumed=<bool>", writes "hello go", prints "GOT <the line read
// back>", closes (close_notify); "CLOSED" at the end. -cert and -key: a
// client certificate for a server that asks
func client(addr, ca, name, curves, alpn, cert, key string, n int, v12 bool, ciphers string) {
	pem, err := os.ReadFile(ca)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	roots := x509.NewCertPool()
	roots.AppendCertsFromPEM(pem)
	cfg := &tls.Config{RootCAs: roots, ServerName: name, MinVersion: tls.VersionTLS13}
	limit(cfg, v12, ciphers)
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
	if cert != "" {
		pair, err := tls.LoadX509KeyPair(cert, key)
		if err != nil {
			fmt.Println("ERROR", err)
			os.Exit(1)
		}
		cfg.Certificates = []tls.Certificate{pair}
	}
	cfg.ClientSessionCache = tls.NewLRUClientSessionCache(8)
	for i := 0; i < n; i++ {
		c, err := tls.Dial("tcp", addr, cfg)
		if err != nil {
			fmt.Println("ERROR", err)
			os.Exit(1)
		}
		st := c.ConnectionState()
		fmt.Printf("STATE %d %d %s resumed=%t version=%x\n", st.CipherSuite, st.CurveID, st.NegotiatedProtocol, st.DidResume, st.Version)
		c.Write([]byte("hello go\n"))
		line, err := bufio.NewReader(c).ReadString('\n')
		if err != nil {
			fmt.Println("ERROR", err)
			os.Exit(1)
		}
		fmt.Printf("GOT %s", line)
		c.Close()
	}
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
	n := flag.Int("n", 1, "the connections, one after another")
	clientauth := flag.String("clientauth", "", "server mode: request or require a client certificate")
	clientca := flag.String("clientca", "", "server mode: the client roots (PEM)")
	v12 := flag.Bool("tls12", false, "TLS 1.2 alone")
	ciphers := flag.String("ciphers", "", "with -tls12: comma-separated TLS 1.2 cipher suite ids (decimal)")
	flag.Parse()
	if *connect != "" {
		client(*connect, *ca, *name, *curves, *alpn, *cert, *key, *n, *v12, *ciphers)
		return
	}
	pair, err := tls.LoadX509KeyPair(*cert, *key)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	cfg := &tls.Config{Certificates: []tls.Certificate{pair}, MinVersion: tls.VersionTLS13}
	limit(cfg, *v12, *ciphers)
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
	if *clientca != "" {
		pem, err := os.ReadFile(*clientca)
		if err != nil {
			fmt.Println("ERROR", err)
			os.Exit(1)
		}
		cfg.ClientCAs = x509.NewCertPool()
		cfg.ClientCAs.AppendCertsFromPEM(pem)
	}
	switch *clientauth {
	case "request":
		cfg.ClientAuth = tls.VerifyClientCertIfGiven
	case "require":
		cfg.ClientAuth = tls.RequireAndVerifyClientCert
	}
	ln, err := tls.Listen("tcp", "127.0.0.1:0", cfg)
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	fmt.Println("LISTEN", ln.Addr().(*net.TCPAddr).Port)
	for i := 0; i < *n; i++ {
		serve(ln)
	}
}

// One connection: the handshake, its state, the lines answered reversed
func serve(ln net.Listener) {
	c, err := ln.Accept()
	if err != nil {
		fmt.Println("ERROR", err)
		os.Exit(1)
	}
	tc := c.(*tls.Conn)
	if err := tc.Handshake(); err != nil {
		fmt.Println("ERROR", err)
		tc.Close()
		return
	}
	st := tc.ConnectionState()
	peer := "-"
	if len(st.PeerCertificates) > 0 {
		peer = strings.ReplaceAll(st.PeerCertificates[0].Subject.CommonName, " ", "_")
	}
	fmt.Printf("STATE %d %d %s resumed=%t peer=%s version=%x\n", st.CipherSuite, st.CurveID, st.NegotiatedProtocol, st.DidResume, peer, st.Version)
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

// TLS 1.2 alone, the suites given
func limit(cfg *tls.Config, v12 bool, ciphers string) {
	if !v12 {
		return
	}
	cfg.MinVersion = tls.VersionTLS12
	cfg.MaxVersion = tls.VersionTLS12
	if ciphers != "" {
		for _, c := range strings.Split(ciphers, ",") {
			var id int
			fmt.Sscan(c, &id)
			cfg.CipherSuites = append(cfg.CipherSuites, uint16(id))
		}
	}
}
