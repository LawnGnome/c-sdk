package newrelic

import (
	"fmt"
	"testing"
)

func TestBasic(t *testing.T) {
	events := newAnalyticsEvents(10)
	events.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})
	events.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})
	events.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})

	id := AgentRunID(`12345`)
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}

	expected := `["12345",` +
		`{"reservoir_size":10,"events_seen":3},` +
		`[[{"x":1},{},{}],` +
		`[{"x":1},{},{}],` +
		`[{"x":1},{},{}]]]`

	if string(json) != expected {
		t.Error(string(json))
	}
	if 3 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 3 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func TestEmpty(t *testing.T) {
	events := newAnalyticsEvents(10)
	id := AgentRunID(`12345`)
	empty := events.Empty()
	if !empty {
		t.Fatal(empty)
	}
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":10,"events_seen":0},[]]` {
		t.Error(string(json))
	}
	if 0 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 0 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func sampleAnalyticsEvent(stamp int) AnalyticsEvent {
	return AnalyticsEvent{
		stamp: eventStamp(stamp),
		data:  []byte(fmt.Sprintf(`{"x":%d}`, stamp)),
	}
}

func TestSampling(t *testing.T) {
	events := newAnalyticsEvents(3)
	events.AddEvent(sampleAnalyticsEvent(10))
	events.AddEvent(sampleAnalyticsEvent(1))
	events.AddEvent(sampleAnalyticsEvent(9))
	events.AddEvent(sampleAnalyticsEvent(2))
	events.AddEvent(sampleAnalyticsEvent(8))
	events.AddEvent(sampleAnalyticsEvent(3))

	id := AgentRunID(`12345`)
	json, err := events.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":3,"events_seen":6},[{"x":8},{"x":10},{"x":9}]]` {
		t.Error(string(json))
	}
	if 6 != events.numSeen {
		t.Error(events.numSeen)
	}
	if 3 != events.NumSaved() {
		t.Error(events.NumSaved())
	}
}

func TestMergeEmpty(t *testing.T) {
	e1 := newAnalyticsEvents(10)
	e2 := newAnalyticsEvents(10)
	e1.Merge(e2)
	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":10,"events_seen":0},[]]` {
		t.Error(string(json))
	}
	if 0 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 0 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
}

func TestMergeFull(t *testing.T) {
	e1 := newAnalyticsEvents(2)
	e2 := newAnalyticsEvents(3)

	e1.AddEvent(sampleAnalyticsEvent(5))
	e1.AddEvent(sampleAnalyticsEvent(10))
	e1.AddEvent(sampleAnalyticsEvent(15))

	e2.AddEvent(sampleAnalyticsEvent(6))
	e2.AddEvent(sampleAnalyticsEvent(12))
	e2.AddEvent(sampleAnalyticsEvent(18))
	e2.AddEvent(sampleAnalyticsEvent(24))

	e1.Merge(e2)
	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":2,"events_seen":7},[{"x":18},{"x":24}]]` {
		t.Error(string(json))
	}
	if 7 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 2 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
}

func TestAnalyticsEventMergeFailedSuccess(t *testing.T) {
	e1 := newAnalyticsEvents(2)
	e2 := newAnalyticsEvents(3)

	e1.AddEvent(sampleAnalyticsEvent(5))
	e1.AddEvent(sampleAnalyticsEvent(10))
	e1.AddEvent(sampleAnalyticsEvent(15))

	e2.AddEvent(sampleAnalyticsEvent(6))
	e2.AddEvent(sampleAnalyticsEvent(12))
	e2.AddEvent(sampleAnalyticsEvent(18))
	e2.AddEvent(sampleAnalyticsEvent(24))

	e1.MergeFailed(e2)

	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":2,"events_seen":7},[{"x":18},{"x":24}]]` {
		t.Error(string(json))
	}
	if 7 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 2 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
	if 1 != e1.failedHarvests {
		t.Error(e1.failedHarvests)
	}
}

func TestAnalyticsEventMergeFailedLimitReached(t *testing.T) {
	e1 := newAnalyticsEvents(2)
	e2 := newAnalyticsEvents(3)

	e1.AddEvent(sampleAnalyticsEvent(5))
	e1.AddEvent(sampleAnalyticsEvent(10))
	e1.AddEvent(sampleAnalyticsEvent(15))

	e2.AddEvent(sampleAnalyticsEvent(6))
	e2.AddEvent(sampleAnalyticsEvent(12))
	e2.AddEvent(sampleAnalyticsEvent(18))
	e2.AddEvent(sampleAnalyticsEvent(24))

	e2.failedHarvests = FailedEventsAttemptsLimit

	e1.MergeFailed(e2)

	id := AgentRunID(`12345`)
	json, err := e1.CollectorJSON(id)
	if nil != err {
		t.Fatal(err)
	}
	if string(json) != `["12345",{"reservoir_size":2,"events_seen":3},[{"x":10},{"x":15}]]` {
		t.Error(string(json))
	}
	if 3 != e1.numSeen {
		t.Error(e1.numSeen)
	}
	if 2 != e1.NumSaved() {
		t.Error(e1.NumSaved())
	}
	if 0 != e1.failedHarvests {
		t.Error(e1.failedHarvests)
	}
}

func BenchmarkEventsCollectorJSON(b *testing.B) {
	data := []byte(`[{"zip":"zap","alpha":"beta","pen":"pencil"},{},{}]`)
	events := NewTxnEvents(MaxTxnEvents)

	for n := 0; n < MaxTxnEvents; n++ {
		events.AddTxnEvent(data)
	}

	id := AgentRunID("12345")

	b.ReportAllocs()
	b.ResetTimer()

	for n := 0; n < b.N; n++ {
		js, err := events.CollectorJSON(id)
		if nil != err {
			b.Fatal(err, js)
		}
	}
}
