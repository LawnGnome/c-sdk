package newrelic

import (
	"testing"
	"time"

	"newrelic/collector"
)

func TestHarvestTriggerGet(t *testing.T) {

	reply := &ConnectReply{}

	trigger := getHarvestTrigger("1234", reply)

	if trigger == nil {
		t.Fatal("No harvest trigger")
	}
}

func TestHarvestTriggerCustom(t *testing.T) {

	triggerChannel := make(chan HarvestType)
	cancelChannel := make(chan bool)
	customTrigger := createFastEventHarvestTrigger(2, 2*time.Millisecond)

	go customTrigger(triggerChannel, cancelChannel)

	// For a harvest trigger than has more than 1 harvest cycle in a given duration, the trigger
	// must first issue a HarvestEvents and then a HarvestAll event
	event1 := (<-triggerChannel)
	event2 := (<-triggerChannel)

	expectedEvent1 := HarvestAll
	expectedEvent2 := HarvestTxnEvents

	if event1 != expectedEvent1 {
		t.Fatal("A HarvestAll harvest trigger event was expected.  Instead:", event1)
	}

	if event2 != expectedEvent2 {
		t.Fatal("A HarvestEvents harvest trigger event was expected.  Instead:", event2)
	}

	cancelChannel <- true
}

func TestHarvestTriggerLicenseLookup(t *testing.T) {

	defaultTrigger := getCustomLicenseHarvestTrigger("1234")
	customTrigger := getCustomLicenseHarvestTrigger("62f73a31951ead399a0c7298001d4118bb907aea")

	if defaultTrigger != nil {
		t.Fatal("Default lookup failed")
	}

	if customTrigger == nil {
		t.Fatal("Custom lookup failed")
	}
}

func TestHarvestTriggerGetCustom(t *testing.T) {

	reply := &ConnectReply{}

	reportPeriod := &collector.ReportPeriod{
		InSeconds: 15,
	}

	methods := &collector.DataMethods{
		ErrorEventData:    reportPeriod,
		AnalyticEventData: reportPeriod,
		CustomEventData:   reportPeriod,
	}

	reply.DataMethods = methods

	trigger := getHarvestTrigger("1234", reply)

	if trigger == nil {
		t.Fatal("No custom harvest trigger created")
	}

}

func TestHarvestTriggerCustomBuilder(t *testing.T) {

	reply := &ConnectReply{}

	methods := &collector.DataMethods{
		ErrorEventData:    &collector.ReportPeriod{InSeconds: 1},
		AnalyticEventData: &collector.ReportPeriod{InSeconds: 1},
		CustomEventData:   &collector.ReportPeriod{InSeconds: 1},
	}

	reply.DataMethods = methods

	triggerChannel := make(chan HarvestType)
	cancelChannel := make(chan bool)
	customTrigger := customTriggerBuilder(reply, collector.MinimumReportPeriod, time.Millisecond)

	go customTrigger(triggerChannel, cancelChannel)

	// Read four HarvestType events.  Each type of HarvestType event should happen once
	m := make(map[HarvestType]int)
	for i := 0; i < 4; i++ {
		event := (<-triggerChannel)
		m[event] = m[event] + 1
	}

	if m[HarvestErrorEvents] != 1 {
		t.Fatal("HarvestErrorEvents event did not occur")
	}

	if m[HarvestTxnEvents] != 1 {
		t.Fatal("HarvestTxnEvents event did not occur")
	}

	if m[HarvestCustomEvents] != 1 {
		t.Fatal("HarvestCustomEvents event did not occur")
	}

	if m[HarvestDefaultData] != 1 {
		t.Fatal("HarvestDefaultData event did not occur")
	}

	cancelChannel <- true

}
