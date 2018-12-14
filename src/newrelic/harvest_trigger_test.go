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

	// PHP-1515: ensure that the cancellation is processed, and that nothing is
	// sent to the trigger channel after cancellation has been confirmed.
	<-cancelChannel

	timer := time.NewTimer(time.Duration(2*collector.MinimumReportPeriod) * time.Millisecond)
	select {
	case <-timer.C:
		// Excellent; we didn't receive anything on triggerChannel.
		break
	case event := <-triggerChannel:
		t.Fatal("Unexpected event %v received after triggers were cancelled", event)
	}
}
