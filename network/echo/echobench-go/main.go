package main

import (
	"bufio"
	"flag"
	"fmt"
	cb "github.com/msaf1980/cyclicbarrier"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"
	"time"
)

type ConnStat struct {
	Conn      net.Conn
	Connected bool
	Count     int
}

var (
	running bool
	b       *cb.CyclicBarrier
	conns   []ConnStat
)

type Config struct {
	Workers     int           // TCP Workers
	Connections int           // number of opened connections, reused in workers
	Send        int           // count of messages, sended in one connection
	Delay       time.Duration // delay beetween send
	Duration    time.Duration // test duration
	ConTimeout  time.Duration // connection timeout
	Timeout     time.Duration // send/recv timeout

	Addr    string
	Stat    string // statfile
	Verbose bool
}

func ParseDurationMin(s string, minDuration time.Duration, name string) (time.Duration, error) {
	duration, err := time.ParseDuration(s)
	if err != nil {
		return 0, fmt.Errorf("Invalid %s value: %s", name, s)
	}
	if duration < minDuration {
		return duration, fmt.Errorf("Low %s value: %s, must be %s or greater", name, duration, minDuration)
	}
	return duration, err
}

func Exists(name string) (bool, error) {
	_, err := os.Stat(name)
	if os.IsNotExist(err) {
		return false, nil
	}
	return err == nil, err
}

func parseArgs() (Config, error) {
	var (
		config Config

		host       string
		port       int
		conTimeout string
		timeout    string
		duration   string
		delay      string
		err        error
	)

	flag.StringVar(&host, "host", "127.0.0.1", "hostname")
	flag.IntVar(&port, "port", 1234, "port")

	flag.IntVar(&config.Workers, "workers", 10, "workers")
	flag.IntVar(&config.Connections, "connections", 0, "number of opened connections, reused in workers, by default - equal to workers count")
	flag.IntVar(&config.Send, "send", 1, "count of messages, sended in one connection")
	flag.StringVar(&delay, "delay", "0", "delay beetween send messages")
	flag.StringVar(&duration, "duration", "60s", "test duration")

	flag.StringVar(&conTimeout, "c", "200ms", "connect timeout")
	flag.StringVar(&timeout, "t", "200ms", "send/recv timeout")

	flag.BoolVar(&config.Verbose, "verbose", false, "verbose")
	flag.StringVar(&config.Stat, "stat", "", "stat file")

	flag.Parse()

	if host == "" {
		host = "127.0.0.1"
	}
	if port < 1 {
		return config, fmt.Errorf("Invalid port value: %d", port)
	}
	config.Addr = fmt.Sprintf("%s:%d", host, port)
	if config.Stat == "" {
		return config, fmt.Errorf("Set stat file")
	}
	if config.Workers < 1 {
		return config, fmt.Errorf("Invalid workers value: %d", config.Workers)
	}
	if config.Connections < 0 {
		return config, fmt.Errorf("Invalid connections value: %d", config.Connections)
	} else if config.Connections == 0 {
		config.Connections = config.Workers
	} else if config.Connections < config.Workers {
		return config, fmt.Errorf("Low connections value: %di, must be equal workers count or greater", config.Connections)
	}
	if config.Send < 1 {
		return config, fmt.Errorf("Invalid send value: %d", config.Send)
	}
	config.Delay, err = ParseDurationMin(delay, 0, "delay")
	if err != nil {
		return config, err
	}
	config.Duration, err = ParseDurationMin(duration, 10*time.Second, "duration")
	if err != nil {
		return config, err
	}
	config.ConTimeout, err = ParseDurationMin(conTimeout, 10*time.Millisecond, "connection timeout")
	if err != nil {
		return config, err
	}
	config.Timeout, err = ParseDurationMin(timeout, 10*time.Millisecond, "timeout")
	if err != nil {
		return config, err
	}

	return config, nil
}

func DumpConfig(w io.Writer, config Config) {
	fmt.Fprintf(w, "#duration: %s\n", config.Duration)
	fmt.Fprintf(w, "#address: %s\n", config.Addr)
	fmt.Fprintf(w, "#workers: %d\n", config.Workers)
	fmt.Fprintf(w, "#connections: %d\n", config.Connections)
	fmt.Fprintf(w, "#send: %d per connection\n", config.Send)
	fmt.Fprintf(w, "#delay: %s\n", config.Delay)
	fmt.Fprintf(w, "#connect timeout: %s\n", config.ConTimeout)
	fmt.Fprintf(w, "#send/recv timeout: %s\n", config.Timeout)

	fmt.Fprintf(w, "#timestamp(ns)\ttesthost\tproto\tremote_address\toper\tduration(us)\tsize\tstatus\n")
}

func main() {
	config, error := parseArgs()
	if error != nil {
		fmt.Printf("%s\n", error)
		os.Exit(1)
	}

	exists, err := Exists(config.Stat)
	if exists {
		fmt.Printf("%s already exists\n", config.Stat)
		os.Exit(1)
	}
	fstat, err := os.OpenFile(config.Stat, os.O_WRONLY|os.O_CREATE, 0644)
	if err != nil {
		log.Fatal(err)
	}
	bwstat := bufio.NewWriterSize(fstat, 65536)
	DumpConfig(bwstat, config)
	bwstat.Flush()

	defer func() {
		fmt.Fprintf(bwstat, "#%s\n", time.Now().Format(time.RFC3339))
		bwstat.Flush()
		fstat.Close()
		for i := 0; i < len(conns); i++ {
			if conns[i].Connected {
				conns[i].Conn.Close()
			}
		}
		log.Println("Shutdown")
	}()

	conns = make([]ConnStat, config.Connections)

	hostname, _ := os.Hostname()

	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)

	running = true
	workers := config.Workers
	result := make(chan Result, config.Workers*10000)
	b = cb.New(config.Workers + 1)
	for i := 0; i < config.Workers; i++ {
		go TcpWorker(i, config, result)
	}
	timer_duration := time.NewTimer(config.Duration)
	b.Await()
	fmt.Fprintf(bwstat, "#%s\n", time.Now().Format(time.RFC3339))
	log.Printf("Starting %d workers, duration %s\n", config.Workers, config.Duration)
LOOP:
	for workers > 0 {
		select {
		case <-sigs:
			running = false
			log.Println("Shutting down with interrupt")
		case <-timer_duration.C:
			running = false
			log.Println("Shutting down")
		case r := <-result:
			if r.Status == Ended {
				workers--
				if workers == 0 {
					break LOOP
				}
			} else {
				// Log format
				// epochtimestamp testhostname proto host:port operation status duration_ms size
				fmt.Fprintf(bwstat, "%d\t%s\t%s\t%s\t%s\t%d\t%d\t%s\n",
					r.Timestamp.UnixNano()/1000000, hostname,
					r.Proto, config.Addr, r.Operation,
					r.Duration.Nanoseconds()/1000, r.Size,
					r.Status)
			}
		}
	}

}
