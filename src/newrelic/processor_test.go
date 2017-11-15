package newrelic

import (
	"errors"
	"testing"
	"time"

	"newrelic/collector"
	"newrelic/log"
	"newrelic/utilization"
)

var (
	appQueryData = []byte(`{
  "license": "4e8c0ad1c4e63058f78cea33765a9ef36004393b",
  "app_name": "PHP Application",
  "agent_language": "php",
  "agent_version": "0.0.1",
  "settings": {"newrelic.analytics_events.capture_attributes":"1"},
  "environment": {},
  "high_security": true,
  "labels": {"shape":"dodecahedron", "color":"green"},
  "redirect_collector":"staging-collector.newrelic.com"
}`)
	idOne = AgentRunID("one")
	idTwo = AgentRunID("two")

	data        = JSONString(`{"age":29}`)
	encoded     = `"eJyqVkpMT1WyMrKsBQQAAP//EVgDDw=="`
	sampleTrace = &TxnTrace{Data: data}

	sampleCustomEvent = []byte("half birthday")
	sampleErrorEvent  = []byte("forgotten birthday")
)

type ClientReturn struct {
	reply []byte
	err   error
}

type ClientParams struct {
	name string
	data []byte
}

type MockedProcessor struct {
	processorHarvestChan chan ProcessorHarvest
	clientReturn         chan ClientReturn
	clientParams         chan ClientParams
	p                    *Processor
}

func NewMockedProcessor(numberOfHarvestPayload int) *MockedProcessor {
	processorHarvestChan := make(chan ProcessorHarvest)
	clientReturn := make(chan ClientReturn)
	clientParams := make(chan ClientParams, numberOfHarvestPayload)

	client := collector.ClientFn(func(cmd collector.Cmd) ([]byte, error) {
		data, err := cmd.Collectible.CollectorJSON(false)
		if nil != err {
			return nil, err
		}
		clientParams <- ClientParams{cmd.Name, data}
		r := <-clientReturn
		return r.reply, r.err
	})

	p := NewProcessor(ProcessorConfig{UseSSL: true, Client: client})
	p.processorHarvestChan = processorHarvestChan
	p.trackProgress = make(chan struct{})
	p.appConnectBackoff = 0
	go p.Run()
	<-p.trackProgress // Wait for utilization

	return &MockedProcessor{
		processorHarvestChan: processorHarvestChan,
		clientReturn:         clientReturn,
		clientParams:         clientParams,
		p:                    p,
	}
}

func (m *MockedProcessor) DoAppInfo(t *testing.T, id *AgentRunID, expectState AppState) {
	reply := m.p.IncomingAppInfo(id, &sampleAppInfo)
	<-m.p.trackProgress // receive app info
	if reply.State != expectState {
		t.Fatal(reply, expectState)
	}
}

func (m *MockedProcessor) DoConnect(t *testing.T, id *AgentRunID) {
	<-m.clientParams // redirect
	m.clientReturn <- ClientReturn{[]byte(`"specific_collector.com"`), nil}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{[]byte(`{"agent_run_id":"` + id.String() + `","zip":"zap"}`), nil}
	<-m.p.trackProgress // receive connect reply
}

func (m *MockedProcessor) TxnData(t *testing.T, id AgentRunID, sample AggregaterInto) {
	m.p.IncomingTxnData(id, sample)
	<-m.p.trackProgress
}

type AggregaterIntoFn func(*Harvest)

func (fn AggregaterIntoFn) AggregateInto(h *Harvest) { fn(h) }

var (
	txnEventSample1 = AggregaterIntoFn(func(h *Harvest) {
		h.TxnEvents.AddTxnEvent([]byte(`[{"x":1},{},{}]`))
	})
	txnEventSample2 = AggregaterIntoFn(func(h *Harvest) {
		h.TxnEvents.AddTxnEvent([]byte(`[{"x":2},{},{}]`))
	})
	txnTraceSample = AggregaterIntoFn(func(h *Harvest) {
		h.TxnTraces.AddTxnTrace(sampleTrace)
	})
	txnCustomEventSample = AggregaterIntoFn(func(h *Harvest) {
		h.CustomEvents.AddEventFromData(sampleCustomEvent)
	})
	txnErrorEventSample = AggregaterIntoFn(func(h *Harvest) {
		h.ErrorEvents.AddEventFromData(sampleErrorEvent)
	})
)

func TestProcessorHarvestDefaultData(t *testing.T) {
	m := NewMockedProcessor(2)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnTraceSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestDefaultData,
	}

	// this code path will trigger two `harvestPayload` calls, so we need
	// to pluck two items out of the clientParams channels
	cp := <-m.clientParams
	cp2 := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice

	toTest := `["one",[[0,0,"","",` + encoded + `,"",null,false,null,null]]]`

	if string(cp.data) != toTest {
		if string(cp2.data) != toTest {
			t.Fatal(string(append(cp.data, cp2.data...)))
		}
	}

	m.p.quit()
}

func TestProcessorHarvestCustomEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnCustomEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestCustomEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[half birthday]]` {
		t.Fatal(string(cp.data))
	}

	m.p.quit()
}

func TestProcessorHarvestErrorEvents(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnErrorEventSample)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestErrorEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":100,"events_seen":1},[forgotten birthday]]` {
		t.Fatal(string(cp.data))
	}

	m.p.quit()
}

func TestForceRestart(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.clientReturn <- ClientReturn{nil, collector.SampleRestartException}
	<-m.p.trackProgress // receive harvest error

	m.DoConnect(t, &idTwo)

	m.DoAppInfo(t, &idOne, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)
	m.TxnData(t, idTwo, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idTwo],
		ID:         idTwo,
		Type:       HarvestTxnEvents,
	}

	<-m.p.trackProgress // receive harvest notice
	cp = <-m.clientParams
	if string(cp.data) != `["two",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.p.quit()
}

func TestDisconnectAtRedirect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // redirect
	m.clientReturn <- ClientReturn{nil, collector.SampleDisonnectException}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.p.quit()
}

func TestLicenseExceptionAtRedirect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // redirect
	m.clientReturn <- ClientReturn{nil, collector.SampleLicenseInvalidException}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateInvalidLicense)

	m.p.quit()
}

func TestDisconnectAtConnect(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // redirect
	m.clientReturn <- ClientReturn{[]byte(`"specific_collector.com"`), nil}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{nil, collector.SampleDisonnectException}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.p.quit()
}

func TestDisconnectAtHarvest(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	// Harvest both (final supportability) metrics and events to trigger
	// multiple calls to processHarvestError, the second call will have an
	// outdated run id.
	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestAll,
	}
	<-m.p.trackProgress // receive harvest notice

	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, collector.SampleDisonnectException}
	<-m.p.trackProgress // receive harvest error

	<-m.clientParams
	m.clientReturn <- ClientReturn{nil, collector.SampleDisonnectException}
	<-m.p.trackProgress // receive harvest error

	m.DoAppInfo(t, nil, AppStateDisconnected)

	m.p.quit()
}

func TestLicenseExceptionAtHarvest(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	cp := <-m.clientParams
	<-m.p.trackProgress // receive harvest notice
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}

	m.clientReturn <- ClientReturn{nil, collector.SampleLicenseInvalidException}
	<-m.p.trackProgress // receive harvest error

	m.DoAppInfo(t, nil, AppStateInvalidLicense)

	m.p.quit()
}

func TestMalformedConnectReply(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // redirect
	m.clientReturn <- ClientReturn{[]byte(`"specific_collector.com"`), nil}
	<-m.clientParams // connect
	m.clientReturn <- ClientReturn{[]byte(`{`), nil}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.p.quit()
}

func TestMalformedCollector(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	<-m.clientParams // redirect
	m.clientReturn <- ClientReturn{[]byte(`"`), nil}
	<-m.p.trackProgress // receive connect reply

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.p.quit()
}

func TestDataSavedOnHarvestError(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, errors.New("unusual error")}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // receive harvest error

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp = <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
}

func TestNoDataSavedOnPayloadTooLarge(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, collector.ErrPayloadTooLarge}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // receive harvest error

	m.TxnData(t, idOne, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp = <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
}

func TestNoDataSavedOnErrUnsupportedMedia(t *testing.T) {
	m := NewMockedProcessor(1)

	m.DoAppInfo(t, nil, AppStateUnknown)

	m.DoConnect(t, &idOne)
	m.DoAppInfo(t, nil, AppStateConnected)

	m.TxnData(t, idOne, txnEventSample1)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp := <-m.clientParams
	m.clientReturn <- ClientReturn{nil, collector.ErrUnsupportedMedia}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":1},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
	<-m.p.trackProgress // receive harvest error

	m.TxnData(t, idOne, txnEventSample2)

	m.processorHarvestChan <- ProcessorHarvest{
		AppHarvest: m.p.harvests[idOne],
		ID:         idOne,
		Type:       HarvestTxnEvents,
	}
	<-m.p.trackProgress // receive harvest notice

	cp = <-m.clientParams
	m.clientReturn <- ClientReturn{nil, nil}
	if string(cp.data) != `["one",{"reservoir_size":10000,"events_seen":1},[[{"x":2},{},{}]]]` {
		t.Fatal(string(cp.data))
	}
}

var (
	id = AgentRunID("12345")

	sampleAppInfo = AppInfo{
		License:           collector.LicenseKey("4e8c0ad1c4e63058f78cea33765a9ef36004393b"),
		Appname:           "PHP Application",
		AgentLanguage:     "php",
		AgentVersion:      "0.0.1",
		Settings:          nil,
		Environment:       nil,
		Labels:            nil,
		RedirectCollector: "staging-collector.newrelic.com",
		HighSecurity:      true,
	}
	connectClient = collector.ClientFn(func(cmd collector.Cmd) ([]byte, error) {
		if cmd.Name == collector.CommandRedirect {
			return []byte(`"specific_collector.com"`), nil
		}
		return []byte(`{"agent_run_id":"12345","zip":"zap"}`), nil
	})
)

func init() {
	log.Init(log.LogAlways, "stdout") // Avoid ssl mismatch warning
}

func TestAppInfoInvalid(t *testing.T) {
	p := NewProcessor(ProcessorConfig{UseSSL: true, Client: collector.LicenseInvalidClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	reply = p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateInvalidLicense || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	p.quit()
}

func TestAppInfoDisconnected(t *testing.T) {
	p := NewProcessor(ProcessorConfig{UseSSL: true, Client: collector.DisconnectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	reply = p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateDisconnected || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	p.quit()
}

func TestAppInfoConnected(t *testing.T) {
	p := NewProcessor(ProcessorConfig{UseSSL: true, Client: connectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	// without agent run id
	reply = p.IncomingAppInfo(nil, &sampleAppInfo)
	if reply.State != AppStateConnected ||
		string(reply.ConnectReply) != `{"agent_run_id":"12345","zip":"zap"}` ||
		reply.RunIDValid {
		t.Fatal(reply)
	}
	// with agent run id
	reply = p.IncomingAppInfo(&id, &sampleAppInfo)
	if !reply.RunIDValid {
		t.Fatal(reply)
	}

	p.quit()
}

func TestAppInfoRunIdOutOfDate(t *testing.T) {
	p := NewProcessor(ProcessorConfig{UseSSL: true, Client: connectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	badID := AgentRunID("bad_id")
	// trigger app creation and connect
	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	<-p.trackProgress // receive app info
	<-p.trackProgress // receive connect reply

	reply = p.IncomingAppInfo(&badID, &sampleAppInfo)
	if reply.State != AppStateConnected || string(reply.ConnectReply) != `{"agent_run_id":"12345","zip":"zap"}` ||
		reply.RunIDValid {
		t.Fatal(reply)
	}
	p.quit()
}

func TestAppInfoInsecureDaemon(t *testing.T) {
	p := NewProcessor(ProcessorConfig{UseSSL: false, Client: connectClient})
	p.processorHarvestChan = nil
	p.trackProgress = make(chan struct{}, 100)
	go p.Run()
	<-p.trackProgress // Wait for utilization

	reply := p.IncomingAppInfo(&id, &sampleAppInfo)
	if reply.State != AppStateUnknown || reply.ConnectReply != nil || reply.RunIDValid {
		t.Fatal(reply)
	}
	if len(p.apps) != 0 {
		t.Fatal(p.apps)
	}
	// this track avoids the processor not being able to quit (because quit() comes in on quitChan)
	<-p.trackProgress // receive app info
	p.quit()
}

func TestShouldConnect(t *testing.T) {
	p := NewProcessor(ProcessorConfig{UseSSL: true, Client: connectClient})
	now := time.Now()
	p.appConnectBackoff = 2 * time.Second

	if p.util != nil {
		t.Error("Utilization should be nil until connected.")
	}

	if p.shouldConnect(&App{state: AppStateUnknown}, now) {
		t.Error("Shouldn't connect app if utilzation data is nil.")
	}

	p.util = &utilization.Data{}
	if !p.shouldConnect(&App{state: AppStateUnknown}, now) {
		t.Error("Should connect app if timeout is valid and app is unknown.")
	}
	if p.shouldConnect(&App{state: AppStateUnknown, lastConnectAttempt: now}, now) ||
		p.shouldConnect(&App{state: AppStateUnknown, lastConnectAttempt: now}, now.Add(time.Second)) {
		t.Error("Shouldn't connect app if last connect attempt was too recent.")
	}
	if !p.shouldConnect(&App{state: AppStateUnknown, lastConnectAttempt: now}, now.Add(3*time.Second)) {
		t.Error("Should connect app if timeout is small enough.")
	}
	if p.shouldConnect(&App{state: AppStateConnected, lastConnectAttempt: now}, now.Add(3*time.Second)) {
		t.Error("Shouldn't connect app if app is already connected.")
	}
}
