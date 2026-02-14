package main

import (
	"encoding/binary"
	"errors"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"
)

const (
	modeBulk  = "bulk"
	modeOnOff = "onoff"
)

type throughputStats struct {
	packets int64
	bytes   int64
}

func (s *throughputStats) add(n int) {
	s.packets++
	s.bytes += int64(n)
}

type sinkOptions struct {
	proto           string
	bind            string
	port            int
	bufferSize      int
	reportIntervalS float64
	listenBacklog   int
	rcvbufBytes     int
	durationS       float64
	startAfterS     float64
}

type sendOptions struct {
	proto           string
	target          string
	port            int
	packetSize      int
	count           int64
	durationS       float64
	pps             float64
	pattern         string
	onMS            float64
	offMS           float64
	flowID          uint32
	reportIntervalS float64
	connectTimeoutS float64
	sndbufBytes     int
	tcpNoDelay      bool
	startAfterS     float64
}

func main() {
	code := run()
	os.Exit(code)
}

func run() int {
	if len(os.Args) < 2 {
		printUsage()
		return 2
	}

	stop := installSignalHandler()
	role := os.Args[1]
	args := os.Args[2:]

	switch role {
	case "sink":
		opts, err := parseSinkArgs(args)
		if err != nil {
			fmt.Fprintf(os.Stderr, "%v\n", err)
			return 2
		}
		if err := validateSinkArgs(opts); err != nil {
			fmt.Fprintf(os.Stderr, "%v\n", err)
			return 2
		}
		if !sleepStartDelay(opts.startAfterS, stop) {
			return 0
		}
		if opts.proto == "udp" {
			if err := runUDPSink(opts, stop); err != nil {
				fmt.Fprintf(os.Stderr, "%v\n", err)
				return 1
			}
			return 0
		}
		if err := runTCPSink(opts, stop); err != nil {
			fmt.Fprintf(os.Stderr, "%v\n", err)
			return 1
		}
		return 0
	case "send":
		opts, err := parseSendArgs(args)
		if err != nil {
			fmt.Fprintf(os.Stderr, "%v\n", err)
			return 2
		}
		if err := validateSendArgs(opts); err != nil {
			fmt.Fprintf(os.Stderr, "%v\n", err)
			return 2
		}
		if !sleepStartDelay(opts.startAfterS, stop) {
			return 0
		}
		if opts.proto == "udp" {
			if err := runUDPSend(opts, stop); err != nil {
				fmt.Fprintf(os.Stderr, "%v\n", err)
				return 1
			}
			return 0
		}
		if err := runTCPSend(opts, stop); err != nil {
			fmt.Fprintf(os.Stderr, "%v\n", err)
			return 1
		}
		return 0
	case "-h", "--help", "help":
		printUsage()
		return 0
	default:
		fmt.Fprintf(os.Stderr, "unknown role: %s\n", role)
		printUsage()
		return 2
	}
}

func installSignalHandler() <-chan struct{} {
	stop := make(chan struct{})
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, os.Interrupt, syscall.SIGTERM)

	var once sync.Once
	go func() {
		<-sigCh
		once.Do(func() { close(stop) })
	}()
	return stop
}

func isStopped(stop <-chan struct{}) bool {
	select {
	case <-stop:
		return true
	default:
		return false
	}
}

func sleepStartDelay(startAfterS float64, stop <-chan struct{}) bool {
	if startAfterS <= 0 {
		return true
	}
	deadline := time.Now().Add(durationFromSeconds(startAfterS))
	for {
		if isStopped(stop) {
			return false
		}
		now := time.Now()
		if !now.Before(deadline) {
			return true
		}
		sleepFor := deadline.Sub(now)
		if sleepFor > 200*time.Millisecond {
			sleepFor = 200 * time.Millisecond
		}
		time.Sleep(sleepFor)
	}
}

func parseSinkArgs(args []string) (sinkOptions, error) {
	fs := flag.NewFlagSet("sink", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	opts := sinkOptions{}
	fs.StringVar(&opts.proto, "proto", "udp", "udp or tcp")
	fs.StringVar(&opts.bind, "bind", "0.0.0.0", "bind address")
	fs.IntVar(&opts.port, "port", 0, "bind port")
	fs.IntVar(&opts.bufferSize, "buffer-size", 65535, "receive buffer per read")
	fs.Float64Var(&opts.reportIntervalS, "report-interval-s", 1.0, "report interval")
	fs.IntVar(&opts.listenBacklog, "listen-backlog", 8, "tcp listen backlog")
	fs.IntVar(&opts.rcvbufBytes, "rcvbuf-bytes", 0, "SO_RCVBUF")
	fs.Float64Var(&opts.durationS, "duration-s", 0.0, "run duration")
	fs.Float64Var(&opts.startAfterS, "start-after-s", 0.0, "startup delay")

	if err := fs.Parse(args); err != nil {
		return sinkOptions{}, err
	}
	if fs.NArg() > 0 {
		return sinkOptions{}, fmt.Errorf("unexpected args: %v", fs.Args())
	}
	return opts, nil
}

func parseSendArgs(args []string) (sendOptions, error) {
	fs := flag.NewFlagSet("send", flag.ContinueOnError)
	fs.SetOutput(io.Discard)

	opts := sendOptions{}
	fs.StringVar(&opts.proto, "proto", "udp", "udp or tcp")
	fs.StringVar(&opts.target, "target", "", "destination address")
	fs.IntVar(&opts.port, "port", 0, "destination port")
	fs.IntVar(&opts.packetSize, "packet-size", 256, "payload size in bytes")
	fs.Int64Var(&opts.count, "count", 1, "number of packets (0 means unlimited)")
	fs.Float64Var(&opts.durationS, "duration-s", 0.0, "run duration")
	fs.Float64Var(&opts.pps, "pps", 0.0, "send pps")
	fs.StringVar(&opts.pattern, "pattern", modeBulk, "traffic pattern")
	fs.Float64Var(&opts.onMS, "on-ms", 2000.0, "on duration in milliseconds")
	fs.Float64Var(&opts.offMS, "off-ms", 1000.0, "off duration in milliseconds")

	flowID := fs.Int("flow-id", 1, "flow id")

	fs.Float64Var(&opts.reportIntervalS, "report-interval-s", 1.0, "report interval")
	fs.Float64Var(&opts.connectTimeoutS, "connect-timeout-s", 3.0, "tcp connect timeout")
	fs.IntVar(&opts.sndbufBytes, "sndbuf-bytes", 0, "SO_SNDBUF")
	fs.BoolVar(&opts.tcpNoDelay, "tcp-nodelay", false, "enable TCP_NODELAY")
	fs.Float64Var(&opts.startAfterS, "start-after-s", 0.0, "startup delay")

	_ = fs.Int("seed", 0, "compat placeholder")
	_ = fs.Int("stream-id", 0, "compat placeholder")
	_ = fs.String("trace-jsonl", "", "compat placeholder")

	if err := fs.Parse(args); err != nil {
		return sendOptions{}, err
	}
	if fs.NArg() > 0 {
		return sendOptions{}, fmt.Errorf("unexpected args: %v", fs.Args())
	}
	if *flowID < 0 {
		return sendOptions{}, errors.New("--flow-id must be >= 0")
	}
	opts.flowID = uint32(*flowID)
	return opts, nil
}

func validateSinkArgs(opts sinkOptions) error {
	if opts.proto != "udp" && opts.proto != "tcp" {
		return errors.New("--proto must be udp or tcp")
	}
	if opts.port <= 0 || opts.port > 65535 {
		return fmt.Errorf("invalid port: %d", opts.port)
	}
	if opts.bufferSize <= 0 {
		return errors.New("--buffer-size must be > 0")
	}
	if opts.reportIntervalS <= 0 {
		return errors.New("--report-interval-s must be > 0")
	}
	if opts.listenBacklog <= 0 {
		return errors.New("--listen-backlog must be > 0")
	}
	if opts.rcvbufBytes < 0 {
		return errors.New("--rcvbuf-bytes must be >= 0")
	}
	if opts.durationS < 0 {
		return errors.New("--duration-s must be >= 0")
	}
	if opts.startAfterS < 0 {
		return errors.New("--start-after-s must be >= 0")
	}
	return nil
}

func validateSendArgs(opts sendOptions) error {
	if opts.proto != "udp" && opts.proto != "tcp" {
		return errors.New("--proto must be udp or tcp")
	}
	if opts.target == "" {
		return errors.New("--target is required")
	}
	if opts.port <= 0 || opts.port > 65535 {
		return fmt.Errorf("invalid port: %d", opts.port)
	}
	if opts.packetSize <= 0 {
		return errors.New("--packet-size must be > 0")
	}
	if opts.count < 0 {
		return errors.New("--count must be >= 0")
	}
	if opts.durationS < 0 {
		return errors.New("--duration-s must be >= 0")
	}
	if opts.pps < 0 {
		return errors.New("--pps must be >= 0")
	}
	if opts.pattern != modeBulk && opts.pattern != modeOnOff {
		return errors.New("--pattern must be bulk or onoff")
	}
	if opts.onMS < 0 {
		return errors.New("--on-ms must be >= 0")
	}
	if opts.offMS < 0 {
		return errors.New("--off-ms must be >= 0")
	}
	if opts.reportIntervalS <= 0 {
		return errors.New("--report-interval-s must be > 0")
	}
	if opts.connectTimeoutS <= 0 {
		return errors.New("--connect-timeout-s must be > 0")
	}
	if opts.sndbufBytes < 0 {
		return errors.New("--sndbuf-bytes must be >= 0")
	}
	if opts.startAfterS < 0 {
		return errors.New("--start-after-s must be >= 0")
	}
	return nil
}

func runUDPSink(opts sinkOptions, stop <-chan struct{}) error {
	addr := net.JoinHostPort(opts.bind, strconv.Itoa(opts.port))
	conn, err := net.ListenPacket("udp", addr)
	if err != nil {
		return err
	}
	defer conn.Close()

	if udpConn, ok := conn.(*net.UDPConn); ok && opts.rcvbufBytes > 0 {
		_ = udpConn.SetReadBuffer(opts.rcvbufBytes)
	}

	stats := throughputStats{}
	start := time.Now()
	last := start
	interval := durationFromSeconds(opts.reportIntervalS)
	durationLimit := durationFromSeconds(opts.durationS)

	fmt.Printf("udp sink listening on %s:%d\n", opts.bind, opts.port)
	buf := make([]byte, opts.bufferSize)

	for {
		if isStopped(stop) {
			break
		}
		now := time.Now()
		if durationLimit > 0 && now.Sub(start) >= durationLimit {
			break
		}

		_ = conn.SetReadDeadline(time.Now().Add(interval))
		n, _, err := conn.ReadFrom(buf)
		if err != nil {
			if ne, ok := err.(net.Error); !ok || !ne.Timeout() {
				return err
			}
		} else {
			stats.add(n)
		}
		if time.Since(last) >= interval {
			last = report("udp sink", start, last, stats)
		}
	}

	report("udp sink final", start, last, stats)
	return nil
}

func runTCPSink(opts sinkOptions, stop <-chan struct{}) error {
	addr := net.JoinHostPort(opts.bind, strconv.Itoa(opts.port))
	listener, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	defer listener.Close()

	stats := throughputStats{}
	start := time.Now()
	last := start
	interval := durationFromSeconds(opts.reportIntervalS)
	durationLimit := durationFromSeconds(opts.durationS)

	fmt.Printf("tcp sink listening on %s:%d\n", opts.bind, opts.port)
	buf := make([]byte, opts.bufferSize)

	for {
		if isStopped(stop) {
			break
		}
		now := time.Now()
		if durationLimit > 0 && now.Sub(start) >= durationLimit {
			break
		}

		if tcpListener, ok := listener.(*net.TCPListener); ok {
			_ = tcpListener.SetDeadline(time.Now().Add(interval))
		}
		conn, err := listener.Accept()
		if err != nil {
			if ne, ok := err.(net.Error); ok && ne.Timeout() {
				if time.Since(last) >= interval {
					last = report("tcp sink", start, last, stats)
				}
				continue
			}
			if errors.Is(err, net.ErrClosed) {
				break
			}
			return err
		}

		peer := conn.RemoteAddr().String()
		fmt.Printf("tcp sink accepted peer=%s\n", peer)
		if tcpConn, ok := conn.(*net.TCPConn); ok && opts.rcvbufBytes > 0 {
			_ = tcpConn.SetReadBuffer(opts.rcvbufBytes)
		}

		for {
			if isStopped(stop) {
				_ = conn.Close()
				break
			}
			if durationLimit > 0 && time.Since(start) >= durationLimit {
				_ = conn.Close()
				break
			}
			_ = conn.SetReadDeadline(time.Now().Add(interval))
			n, err := conn.Read(buf)
			if err != nil {
				if ne, ok := err.(net.Error); ok && ne.Timeout() {
					if time.Since(last) >= interval {
						last = report("tcp sink", start, last, stats)
					}
					continue
				}
				if errors.Is(err, io.EOF) {
					fmt.Printf("tcp sink peer closed peer=%s\n", peer)
					_ = conn.Close()
					break
				}
				_ = conn.Close()
				return err
			}
			if n > 0 {
				stats.add(n)
			}
			if time.Since(last) >= interval {
				last = report("tcp sink", start, last, stats)
			}
		}
	}

	report("tcp sink final", start, last, stats)
	return nil
}

func runUDPSend(opts sendOptions, stop <-chan struct{}) error {
	target := net.JoinHostPort(opts.target, strconv.Itoa(opts.port))
	raddr, err := net.ResolveUDPAddr("udp", target)
	if err != nil {
		return err
	}
	conn, err := net.DialUDP("udp", nil, raddr)
	if err != nil {
		return err
	}
	defer conn.Close()

	if opts.sndbufBytes > 0 {
		_ = conn.SetWriteBuffer(opts.sndbufBytes)
	}

	fmt.Printf(
		"udp send target=%s:%d packet_size=%d count=%d duration_s=%.3f pps=%.3f pattern=%s\n",
		opts.target,
		opts.port,
		opts.packetSize,
		opts.count,
		opts.durationS,
		opts.pps,
		opts.pattern,
	)
	return runSendLoop(opts, stop, conn.Write, "udp")
}

func runTCPSend(opts sendOptions, stop <-chan struct{}) error {
	target := net.JoinHostPort(opts.target, strconv.Itoa(opts.port))
	dialer := net.Dialer{Timeout: durationFromSeconds(opts.connectTimeoutS)}
	connRaw, err := dialer.Dial("tcp", target)
	if err != nil {
		return err
	}
	defer connRaw.Close()

	if tcpConn, ok := connRaw.(*net.TCPConn); ok {
		if opts.tcpNoDelay {
			_ = tcpConn.SetNoDelay(true)
		}
		if opts.sndbufBytes > 0 {
			_ = tcpConn.SetWriteBuffer(opts.sndbufBytes)
		}
	}

	fmt.Printf(
		"tcp send connected target=%s:%d packet_size=%d count=%d duration_s=%.3f pps=%.3f pattern=%s\n",
		opts.target,
		opts.port,
		opts.packetSize,
		opts.count,
		opts.durationS,
		opts.pps,
		opts.pattern,
	)
	return runSendLoop(opts, stop, connRaw.Write, "tcp")
}

func runSendLoop(
	opts sendOptions,
	stop <-chan struct{},
	sendFn func([]byte) (int, error),
	prefix string,
) error {
	stats := throughputStats{}
	start := time.Now()
	last := start
	interval := durationFromSeconds(opts.reportIntervalS)
	durationLimit := durationFromSeconds(opts.durationS)

	onDuration := durationFromSeconds(opts.onMS / 1000.0)
	offDuration := durationFromSeconds(opts.offMS / 1000.0)

	nextSend := start
	for {
		if isStopped(stop) {
			break
		}
		now := time.Now()
		if opts.count > 0 && stats.packets >= opts.count {
			break
		}
		if durationLimit > 0 && now.Sub(start) >= durationLimit {
			break
		}

		sendAt, ok := computeNextSendTime(
			opts,
			start,
			now,
			nextSend,
			onDuration,
			offDuration,
		)
		if !ok {
			break
		}
		if sendAt.After(now) {
			sleepFor := sendAt.Sub(now)
			if sleepFor > 200*time.Millisecond {
				sleepFor = 200 * time.Millisecond
			}
			time.Sleep(sleepFor)
			if time.Since(last) >= interval {
				last = report(prefix+" send", start, last, stats)
			}
			continue
		}

		payload := buildPayload(opts.packetSize, opts.flowID, uint64(stats.packets+1))
		n, err := sendFn(payload)
		if err != nil {
			return err
		}
		stats.add(n)

		if opts.pps > 0 {
			nextSend = sendAt.Add(durationFromSeconds(1.0 / opts.pps))
		} else {
			nextSend = time.Now()
		}
		if time.Since(last) >= interval {
			last = report(prefix+" send", start, last, stats)
		}
	}

	report(prefix+" send final", start, last, stats)
	return nil
}

func computeNextSendTime(
	opts sendOptions,
	start time.Time,
	now time.Time,
	nextSend time.Time,
	onDuration time.Duration,
	offDuration time.Duration,
) (time.Time, bool) {
	candidate := now
	if opts.pps > 0 && nextSend.After(candidate) {
		candidate = nextSend
	}
	if opts.pattern == modeBulk {
		return candidate, true
	}
	return clampToOnWindow(candidate, start, onDuration, offDuration)
}

func clampToOnWindow(
	ts time.Time,
	start time.Time,
	onDuration time.Duration,
	offDuration time.Duration,
) (time.Time, bool) {
	if onDuration <= 0 && offDuration > 0 {
		return time.Time{}, false
	}
	cycle := onDuration + offDuration
	if cycle <= 0 {
		return ts, true
	}
	elapsed := ts.Sub(start)
	if elapsed < 0 {
		elapsed = 0
	}
	offset := elapsed % cycle
	if offset < onDuration {
		return ts, true
	}
	return ts.Add(cycle - offset), true
}

func buildPayload(packetSize int, flowID uint32, seq uint64) []byte {
	sendTS := uint64(time.Now().UnixNano())
	const headerLen = 28

	if packetSize <= 0 {
		return nil
	}
	if packetSize < headerLen {
		marker := []byte(fmt.Sprintf("%012d", seq))
		out := make([]byte, packetSize)
		for i := range out {
			out[i] = marker[i%len(marker)]
		}
		return out
	}

	payloadLen := packetSize - headerLen
	out := make([]byte, packetSize)
	copy(out[0:4], []byte("RMM1"))
	binary.BigEndian.PutUint32(out[4:8], flowID)
	binary.BigEndian.PutUint64(out[8:16], seq)
	binary.BigEndian.PutUint64(out[16:24], sendTS)
	binary.BigEndian.PutUint32(out[24:28], uint32(payloadLen))
	for i := 28; i < len(out); i++ {
		out[i] = 'x'
	}
	return out
}

func report(prefix string, start time.Time, last time.Time, stats throughputStats) time.Time {
	now := time.Now()
	elapsed := now.Sub(start).Seconds()
	interval := now.Sub(last).Seconds()
	if elapsed < 1e-9 {
		elapsed = 1e-9
	}
	if interval < 1e-9 {
		interval = 1e-9
	}
	avgPPS := float64(stats.packets) / elapsed
	avgMbps := (float64(stats.bytes) * 8.0) / elapsed / 1_000_000.0
	fmt.Printf(
		"%s elapsed=%.3fs packets=%d bytes=%d avg_pps=%.2f avg_mbps=%.3f interval=%.3fs\n",
		prefix,
		elapsed,
		stats.packets,
		stats.bytes,
		avgPPS,
		avgMbps,
		interval,
	)
	return now
}

func durationFromSeconds(value float64) time.Duration {
	if value <= 0 {
		return 0
	}
	return time.Duration(value * float64(time.Second))
}

func printUsage() {
	fmt.Println("usage: traffic_app {sink|send} [options]")
	fmt.Println("")
	fmt.Println("sink options:")
	fmt.Println("  --proto udp|tcp")
	fmt.Println("  --bind 0.0.0.0 --port <port>")
	fmt.Println("  --buffer-size 65535 --report-interval-s 1")
	fmt.Println("  --listen-backlog 8 --rcvbuf-bytes 0")
	fmt.Println("  --duration-s 0 --start-after-s 0")
	fmt.Println("")
	fmt.Println("send options:")
	fmt.Println("  --proto udp|tcp --target <ip> --port <port>")
	fmt.Println("  --packet-size 256 --count 1 --duration-s 0 --pps 0")
	fmt.Println("  --pattern bulk|onoff --on-ms 2000 --off-ms 1000")
	fmt.Println("  --flow-id 1 --report-interval-s 1")
	fmt.Println("  --connect-timeout-s 3 --sndbuf-bytes 0 --tcp-nodelay")
	fmt.Println("  --start-after-s 0")
}
