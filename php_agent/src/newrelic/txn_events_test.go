package newrelic

import "testing"

func BenchmarkTxnEvents(b *testing.B) {
	data := []byte(`[{"zip":"zap","alpha":"beta","pen":"pencil"},{},{}]`)
	events := NewTxnEvents(MaxTxnEvents)

	b.ReportAllocs()

	for n := 0; n < b.N; n++ {
		events.AddTxnEvent(data)
	}
}
