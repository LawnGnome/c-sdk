package newrelic

import (
	"reflect"
	"testing"
	"time"
)

// A MockedAppHarvest comprises two groups of data.  First, the dependencies necessary to build out an AppHarvest.
// Second, the AppHarvest built from said dependencies using NewMockedAppHarvest().
type MockedAppHarvest struct {
	*App
	processorHarvestChan chan ProcessorHarvest
	harvestsPerCycle     int
	cycleDuration        time.Duration
	ah                   *AppHarvest
}

func (m *MockedAppHarvest) NewMockedAppHarvest() {
	harvest := NewHarvest(time.Now())

	m.App.HarvestTrigger = createFastEventHarvestTrigger(m.harvestsPerCycle, m.cycleDuration)

	m.ah = NewAppHarvest(AgentRunID("1234"), m.App, harvest, m.processorHarvestChan)
}

func TestAppHarvestMessageTransformation(t *testing.T) {
	m := &MockedAppHarvest{
		App:                  &App{},
		processorHarvestChan: make(chan ProcessorHarvest),
		harvestsPerCycle:     1,
		cycleDuration:        1 * time.Minute,
	}

	m.NewMockedAppHarvest()

	expectedEvent := ProcessorHarvest{
		AppHarvest: m.ah,
		ID:         "1234",
		Type:       HarvestAll,
	}

	actualEvent := m.ah.NewProcessorHarvestEvent("1234", HarvestAll)

	if !reflect.DeepEqual(expectedEvent, actualEvent) {
		t.Fatal("The Processor Harvest Event was not created as expected")
	}

	m.ah.Close()
}

func TestAppHarvestTrigger(t *testing.T) {
	m := &MockedAppHarvest{
		App:                  &App{},
		processorHarvestChan: make(chan ProcessorHarvest),
		harvestsPerCycle:     2,
		cycleDuration:        2 * time.Millisecond,
	}

	m.NewMockedAppHarvest()

	_, ok := (<-m.processorHarvestChan)

	if !ok {
		t.Fatal("AppHarvest trigger not ok")
	}

	m.ah.Close()
}

func TestAppHarvestClose(t *testing.T) {
	m := &MockedAppHarvest{
		App:                  &App{},
		processorHarvestChan: make(chan ProcessorHarvest),
		harvestsPerCycle:     1,
		cycleDuration:        1 * time.Minute,
	}

	m.NewMockedAppHarvest()

	m.ah.Close()

	// Attempt to read from the AppHarvest's HarvestChannel, trigger
	_, ok := (<-m.ah.trigger)

	if ok {
		t.Fatal("AppHarvest not closed")
	}

	// Attempt to read from the AppHarvest's bool channel, cancel
	_, ok = (<-m.ah.cancel)

	if ok {
		t.Fatal("AppHarvest not closed")
	}
}
