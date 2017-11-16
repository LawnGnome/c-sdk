package newrelic

import (
	"testing"
	"time"
)

func TestCreateFinalMetricsWithLotsOfMetrics(t *testing.T) {
	harvest := NewHarvest(time.Date(2015, time.November, 11, 1, 2, 0, 0, time.UTC))

	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.TxnEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"z":42},{},{}]`), stamp: eventStamp(1)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})
	harvest.CustomEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"x":1},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})
	harvest.ErrorEvents.AddEvent(AnalyticsEvent{data: []byte(`[{"y":5},{},{}]`), stamp: eventStamp(1)})

	harvest.createFinalMetrics()

	var expectedJSON = `["12345",1447203720,1417136520,` +
		`[[{"name":"Instance/Reporting"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[8,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[4,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[7,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[7,0,0,0,0,0]]]]`

	json, err := harvest.Metrics.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if got := string(json); got != expectedJSON {
		t.Errorf("got=%q want=%q", got, expectedJSON)
	}
}

func TestCreateFinalMetricsWithNoMetrics(t *testing.T) {
	harvest := NewHarvest(time.Date(2015, time.November, 11, 1, 2, 0, 0, time.UTC))
	harvest.pidSet[0] = struct{}{}
	harvest.createFinalMetrics()

	var expectedJSON = `["12345",1447203720,1417136520,` +
		`[[{"name":"Instance/Reporting"},[1,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSeen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/AnalyticsEvents/TotalEventsSent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/Customer/Sent"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Seen"},[0,0,0,0,0,0]],` +
		`[{"name":"Supportability/Events/TransactionError/Sent"},[0,0,0,0,0,0]]]]`

	json, err := harvest.Metrics.CollectorJSONSorted(AgentRunID(`12345`), end)
	if nil != err {
		t.Fatal(err)
	}
	if got := string(json); got != expectedJSON {
		t.Errorf("got=%q want=%q", got, expectedJSON)
	}
}

func TestHarvestEmpty(t *testing.T) {
	startTime := time.Date(2015, time.November, 11, 1, 2, 0, 0, time.UTC)

	if !NewHarvest(startTime).empty() {
		t.Errorf("NewHarvest().empty() = false, want true")
	}

	var h *Harvest

	h = NewHarvest(startTime)
	h.pidSet[0] = struct{}{}
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.CustomEvents.AddEvent(AnalyticsEvent{stamp: 42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.ErrorEvents.AddEvent(AnalyticsEvent{stamp: 42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.Errors.AddError(42, []byte{})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.Metrics.AddCount("WebTransaction", "", 1, Forced)
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.SlowSQLs.Observe(&SlowSQL{})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.TxnEvents.AddEvent(AnalyticsEvent{stamp: 42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}

	h = NewHarvest(startTime)
	h.TxnTraces.AddTxnTrace(&TxnTrace{DurationMillis: 42})
	if h.empty() {
		t.Errorf("Harvest.empty() = true, want false")
	}
}
