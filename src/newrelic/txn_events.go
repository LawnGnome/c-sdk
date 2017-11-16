package newrelic

import (
	"math/rand"
)

// TxnEvents is a wrapper over AnalyticsEvents created for additional type
// safety and proper FailedHarvest behavior.
type TxnEvents struct {
	*analyticsEvents
}

// NewTxnEvents returns a new transaction event reservoir with capacity max.
func NewTxnEvents(max int) *TxnEvents {
	return &TxnEvents{newAnalyticsEvents(max)}
}

// AddTxnEvent observes the occurence of a transaction event. If the
// reservoir is full, sampling occurs. Note: when sampling occurs, it
// is possible the new event may be discarded.
func (events *TxnEvents) AddTxnEvent(data []byte) {
	stamp := eventStamp(rand.Float32())
	events.AddEvent(AnalyticsEvent{data: data, stamp: stamp})
}

// AddSyntheticsEvent observes the occurence of a Synthetics
// transaction event. If the reservoir is full, sampling occurs. Note:
// when sampling occurs, it is possible the new event may be
// discarded.
func (events *TxnEvents) AddSyntheticsEvent(data []byte) {
	// Synthetics events always get priority
	stamp := eventStamp(1 + rand.Float32())
	events.AddEvent(AnalyticsEvent{data: data, stamp: stamp})
}

// FailedHarvest is a callback invoked by the processor when an
// attempt to deliver the contents of events to the collector
// fails. After a failed delivery attempt, events is merged into
// the upcoming harvest. This may result in sampling.
func (events *TxnEvents) FailedHarvest(newHarvest *Harvest) {
	newHarvest.TxnEvents.MergeFailed(events.analyticsEvents)
}
