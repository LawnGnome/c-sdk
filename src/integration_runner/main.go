package main

import (
	"bytes"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"sync"
	"time"

	"newrelic"
	"newrelic/integration"
	"newrelic/log"
	"newrelic/utilization"
)

var (
	flagAgent     = flag.String("agent", "", "")
	flagCGI       = flag.String("cgi", "", "")
	flagCollector = flag.String("collector", "", "the collector host")
	flagInsecure  = flag.Bool("insecure", false, "disable TLS")
	flagJunit     = flag.String("junit", "", "")
	flagLoglevel  = flag.String("loglevel", "", "agent log level")
	flagOutputDir = flag.String("output-dir", ".", "")
	flagPHP       = flag.String("php", "", "")
	flagPort      = flag.String("port", defaultPort(), "")
	flagRetry     = flag.Int("retry", 0, "maximum retry attempts")
	flagTimeout   = flag.Duration("timeout", 10*time.Second, "")
	flagValgrind  = flag.String("valgrind", "", "if given, this is the path to valgrind")
	flagWorkers   = flag.Int("threads", 1, "")

	// externalPort is the port on which we start a server to handle
	// external calls.
	flagExternalPort = flag.Int("external_port", 0, "")
)

// Default directories to search for tests.
var defaultArgs = []string{
	"tests/integration",
	"tests/library",
	"tests/regression",
}

// PHP Test Account 1
var (
	AccountNumber = 432507
	InsightsKey   = "PFUes8SAU0SfThhL04cVzGMKTMj0iuub"
	InsightsHost  = "staging-insights-collector.newrelic.com"

	TestApp = newrelic.AppInfo{
		// Changing the application will break the cross process ids in
		// the cross application trace tests:  '432507#73695'
		//
		// Note:  If this is a problem, we could swap the
		// 'cross_process_id' in the connect response JSON with a fixed
		// value.
		License:           "07a2ad66c637a29c3982469a3fe8d1982d002c4a",
		Appname:           "PHP Agent Integration Tests",
		RedirectCollector: "staging-collector.newrelic.com",
		AgentVersion:      "0",
		AgentLanguage:     "php",
		HighSecurity:      false,
		Environment:       nil,
		Labels:            nil,
		Settings:
		// Ensure that we get Javascript agent code in the reply
		newrelic.JSONString(`{"newrelic.browser_monitoring.debug":false,"newrelic.browser_monitoring.loader":"rum"}`),
	}

	commonSettings map[string]string
)

var ctx *integration.Context

func defaultPort() string {
	name := fmt.Sprintf("newrelic-daemon-%d.sock", os.Getpid())
	name = filepath.Join(os.TempDir(), name)
	if runtime.GOOS == "linux" {
		// Use an abstract domain socket.
		name = "@" + name
	}
	return name
}

func merge(a, b map[string]string) map[string]string {
	merged := make(map[string]string)
	for k, v := range a {
		merged[k] = v
	}
	for k, v := range b {
		merged[k] = v
	}
	return merged
}

func catRequest(w http.ResponseWriter, r *http.Request) {
	catFile := r.URL.Query().Get("file")
	if "" == catFile {
		http.Error(w, "cat failure: no file provided", http.StatusBadRequest)
		return
	}

	env := merge(ctx.Env, nil)
	settings := merge(ctx.Settings, nil)
	settings["newrelic.appname"] = "ignore"

	tx, err := integration.CgiTx(integration.ScriptFile(catFile), env, settings, r.Header, ctx)
	if nil != err {
		http.Error(w, "cat failure: "+err.Error(), http.StatusInternalServerError)
		return
	}

	headers, body, err := tx.Execute()
	if nil != err {
		http.Error(w, "cat failure: "+err.Error(), http.StatusInternalServerError)
		return
	}

	// Copy response headers
	h := w.Header()
	for key, vals := range headers {
		for _, val := range vals {
			h.Add(key, val)
		}
	}

	w.Write(body)
}

func main() {
	// Respect GOMAXPROCS if set; otherwise, use all available CPUs.
	if os.Getenv("GOMAXPROCS") == "" {
		runtime.GOMAXPROCS(runtime.NumCPU())
	}

	flag.Parse()

	// Set the redirect collector from the flag, if given.
	if *flagCollector != "" {
		TestApp.RedirectCollector = *flagCollector
	}

	if *flagPHP == "" {
		*flagPHP = "php"
	}

	if len(*flagCGI) == 0 {
		if len(*flagPHP) > 0 {
			*flagCGI = stringReplaceLast(*flagPHP, "php", "php-cgi", 1)
		} else {
			*flagCGI = "php-cgi"
		}
	}

	var err error
	*flagCGI, err = exec.LookPath(*flagCGI)
	if nil != err {
		fmt.Fprintf(os.Stderr, "unable to find cgi: %v\n", err)
		os.Exit(1)
	}

	// Start server for external requests.
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		io.WriteString(w, "Hello world!")
	})
	mux.HandleFunc("/cat", catRequest)
	addr := "127.0.0.1:" + strconv.Itoa(*flagExternalPort)
	srv := &http.Server{Addr: addr, Handler: mux}
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		fmt.Fprintf(os.Stderr, "unable start external server: %v\n", err)
		os.Exit(1)
	}
	externalHost := ln.Addr().String()
	go func() {
		err := srv.Serve(ln)
		if nil != err {
			fmt.Fprintf(os.Stderr, "unable serve external server: %v\n", err)
			os.Exit(1)
		}
	}()

	if len(*flagPort) == 0 {
		*flagPort = defaultPort()
	}

	*flagOutputDir, _ = filepath.Abs(*flagOutputDir)
	daemonLog := filepath.Join(*flagOutputDir, "integration-tests.log")
	agentLog := filepath.Join(*flagOutputDir, "php_agent.log")
	os.Remove(daemonLog)
	os.Remove(agentLog)

	log.Init(log.LogDebug, daemonLog)

	ctx = integration.NewContext(*flagPHP, *flagCGI)
	ctx.Valgrind = *flagValgrind
	ctx.Timeout = *flagTimeout

	// Settings common to all tests.
	ctx.Settings = make(map[string]string)
	ctx.Settings["newrelic.license"] = string(TestApp.License)
	ctx.Settings["newrelic.logfile"] = agentLog
	ctx.Settings["newrelic.daemon.port"] = `"` + *flagPort + `"`
	ctx.Settings["newrelic.daemon.dont_launch"] = "3"
	ctx.Settings["newrelic.special"] = "debug_cat"

	if *flagLoglevel != "" {
		ctx.Settings["newrelic.loglevel"] = *flagLoglevel
	}

	// If the user provided a custom agent extension, use it.
	if len(*flagAgent) > 0 {
		ctx.Settings["extension"], _ = filepath.Abs(*flagAgent)
	}

	// Env vars common to all tests.
	ctx.Env["EXTERNAL_HOST"] = externalHost

	// TODO: support tcp
	handler, err := startDaemon("unix", *flagPort)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}

	args := flag.Args()
	if 0 == len(args) {
		args = defaultArgs
	}

	testFiles := discoverTests("test_*.php", args)
	tests := make([]*integration.Test, 0, len(testFiles))
	testsToRun := make(chan *integration.Test, len(testFiles))
	for _, filename := range testFiles {
		test := integration.ParseTestFile(filename)
		tests = append(tests, test)
		testsToRun <- test
	}

	runTests(testsToRun, *flagWorkers)

	// Now wait for all data to be flushed.
	// TODO: Make this intelligent by gracefully shutting down the listener.
	// Once graceful shutdown is supported, manually deleting the sock
	// file will no longer be required.
	time.Sleep(50 * time.Millisecond)

	for i := 0; i < *flagRetry; i++ {
		testsToRetry := make(chan *integration.Test, len(testFiles))

		handler.Lock()
		for _, tc := range tests {
			if !tc.Failed && !tc.Skipped {
				if handler.harvests[tc.Name] == nil {
					testsToRetry <- tc
				}
			}
		}
		handler.Unlock()

		if len(testsToRetry) == 0 {
			break
		}

		retryTests(testsToRetry, *flagWorkers)
		time.Sleep(50 * time.Millisecond)
	}

	deleteSockfile("unix", *flagPort)

	var numFailed int

	// Compare the output
	handler.Lock()
	for _, tc := range tests {
		if !tc.Failed && !tc.Skipped {
			tc.Compare(handler.harvests[tc.Name])
		}
		if tc.Failed && tc.Xfail == "" {
			numFailed++
		}
	}

	tapOutput(tests)

	if numFailed > 0 {
		os.Exit(1)
	}
}

var (
	skipRE  = regexp.MustCompile(`^(?i)\s*skip`)
	xfailRE = regexp.MustCompile(`^(?i)\s*xfail`)
)

func runTests(testsToRun chan *integration.Test, numWorkers int) {
	var wg sync.WaitGroup

	for i := 0; i < numWorkers; i++ {
		wg.Add(1)

		go func() {
			defer wg.Done()
			for {
				select {
				case tc := <-testsToRun:
					fmt.Println("running", tc.Name)
					// Note that runTest will modify test
					// fields.  These will be visible to the
					// main goroutine because of the
					// wg.Done() call.
					runTest(tc)
				default:
					return
				}
			}
		}()
	}

	wg.Wait()
}

func retryTests(testsToRun chan *integration.Test, numWorkers int) {
	var wg sync.WaitGroup

	for i := 0; i < numWorkers; i++ {
		wg.Add(1)

		go func() {
			defer wg.Done()
			for {
				select {
				case tc := <-testsToRun:
					fmt.Println("retrying", tc.Name)
					// Note that runTest will modify test
					// fields.  These will be visible to the
					// main goroutine because of the
					// wg.Done() call.
					tc.Reset()
					runTest(tc)
				default:
					return
				}
			}
		}()
	}

	wg.Wait()
}

func runTest(t *integration.Test) {
	if nil != t.Err {
		return
	}

	skipIf, _ := t.MakeSkipIf(ctx)
	if skipIf != nil {
		_, body, err := skipIf.Execute()

		if err != nil {
			t.Output = body
			t.Fatal(fmt.Errorf("error executing skipif: %v", err))
			return
		}

		if skipRE.Match(body) {
			reason := string(bytes.TrimSpace(head(body)))
			t.Skip(reason)
			return
		}
	}

	run, _ := t.MakeRun(ctx)
	_, body, err := run.Execute()

	// Always save the test output. If an error occurred it may contain
	// critical information regarding the cause. Currently, it may also
	// contain valgrind commentary which we want to display.
	t.Output = body

	if err != nil {
		if _, ok := err.(*exec.ExitError); !ok {
			t.Fatal(fmt.Errorf("error executing script: %v", err))
			return
		}
	}

	if skipRE.Match(body) {
		reason := string(bytes.TrimSpace(head(body)))
		t.Skip(reason)
		return
	}

	if xfailRE.Match(body) {
		// Strip xfail message from body so it does not affect expectations.
		tmp := bytes.SplitN(body, []byte("\n"), 2)
		t.XFail(string(bytes.TrimSpace(tmp[0])))
		if len(tmp) == 2 {
			body = tmp[1]
		}
	}
}

// head returns the first line of s.
func head(s []byte) []byte {
	if i := bytes.IndexByte(s, '\n'); i >= 0 {
		return s[:i+1]
	}
	return s
}

// discoverTests recursively searches the paths in searchPaths and
// returns the paths of each file that matches pattern.
func discoverTests(pattern string, searchPaths []string) []string {
	testFiles := make([]string, 0, 100)

	for _, root := range searchPaths {
		if info, err := os.Stat(root); err == nil && info.Mode().IsRegular() {
			testFiles = append(testFiles, root)
			continue
		}

		filepath.Walk(root, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				// TODO: Log errors during walk?
				return nil
			}

			if info.Mode().IsRegular() {
				if ok, _ := filepath.Match(pattern, info.Name()); ok {
					testFiles = append(testFiles, path)
				}
			}

			return nil
		})
	}

	return testFiles
}

func substituteAgentRunID(reply []byte, newRunID string) []byte {
	var x map[string]interface{}

	// TODO deal with errors
	json.Unmarshal(reply, &x)

	x["agent_run_id"] = newRunID

	out, _ := json.Marshal(x)
	return out
}

type IntegrationDataHandler struct {
	sync.Mutex                              // Protects harvests
	harvests   map[string]*newrelic.Harvest // Keyed by tc.Name (which is used as AgentRunID)
	reply      []byte                       // Constant after creation
}

func (h *IntegrationDataHandler) IncomingTxnData(id newrelic.AgentRunID, sample newrelic.AggregaterInto) {
	h.Lock()
	defer h.Unlock()

	harvest := h.harvests[string(id)]
	if nil == harvest {
		harvest = newrelic.NewHarvest(time.Now())
		// Save a little memory by reducing the event pools.
		harvest.TxnEvents = newrelic.NewTxnEvents(50)
		harvest.CustomEvents = newrelic.NewCustomEvents(50)
		h.harvests[string(id)] = harvest
	}

	sample.AggregateInto(harvest)
}

func (h *IntegrationDataHandler) IncomingAppInfo(id *newrelic.AgentRunID, info *newrelic.AppInfo) newrelic.AppInfoReply {
	return newrelic.AppInfoReply{
		State: newrelic.AppStateConnected,
		// Use the appname (which has been set to the filename) as
		// the agent run id to enable running the tests in
		// parallel.
		ConnectReply: substituteAgentRunID(h.reply, info.Appname),
	}
}

func deleteSockfile(network, address string) {
	if network == "unix" && !(address[0] == '@') {
		err := os.Remove(address)
		if err != nil && !os.IsNotExist(err) {
			fmt.Fprintln(os.Stderr, "unable to remove stale sock file: %v"+
				" - another daemon may already be running?", err)
		}
	}
}

// startDaemon bootstraps the daemon components required to run the
// tests. There are two types of messages an agent can send that affect
// the integration tests: appinfo queries, and txndata. Normally, these
// would be handled by newrelic.Process. We do not do so here, instead
// the test runner intercepts these messages for inspection. This has
// the side-effect of disabling the harvest.
//
// Note: with a little refactoring in the daemon we could continue to
// stub out appinfo queries and inspect txndata while preserving the
// harvest.
func startDaemon(network, address string) (*IntegrationDataHandler, error) {
	// TODO(msl): Do we need to gather utilization data during integration tests?
	client, _ := newrelic.NewClient(&newrelic.ClientConfig{})
	connectData, err := TestApp.ConnectJSON(utilization.Gather(
		utilization.Config{
			DetectAWS:    true,
			DetectAzure:  true,
			DetectGCP:    true,
			DetectPCF:    true,
			DetectDocker: true,
		}))
	if err != nil {
		return nil, fmt.Errorf("unable to connect application: %v", err)
	}

	connectAttempt := newrelic.ConnectApplication(&newrelic.ConnectArgs{
		RedirectCollector: TestApp.RedirectCollector,
		Payload:           connectData,
		License:           TestApp.License,
		UseSSL:            !(*flagInsecure),
		Client:            client,
	})

	if nil != connectAttempt.Err {
		return nil, fmt.Errorf("unable to connect application: %v", connectAttempt.Err)
	}

	handler := &IntegrationDataHandler{
		reply:    connectAttempt.RawReply,
		harvests: make(map[string]*newrelic.Harvest),
	}

	go func() {
		deleteSockfile(network, address) // in case there's a stale one hanging around.

		err := newrelic.ListenAndServe(network, address, newrelic.CommandsHandler{Processor: handler})
		if nil != err {
			deleteSockfile(network, address)
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
	}()

	// TODO: Ensure listener is ready without guesswork.
	time.Sleep(50 * time.Millisecond)

	return handler, nil
}
