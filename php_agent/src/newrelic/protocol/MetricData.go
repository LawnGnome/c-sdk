// automatically generated, do not modify

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type MetricData struct {
	_tab flatbuffers.Struct
}

func (rcv *MetricData) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *MetricData) Count() float64 {
	return rcv._tab.GetFloat64(rcv._tab.Pos + flatbuffers.UOffsetT(0))
}
func (rcv *MetricData) Total() float64 {
	return rcv._tab.GetFloat64(rcv._tab.Pos + flatbuffers.UOffsetT(8))
}
func (rcv *MetricData) Exclusive() float64 {
	return rcv._tab.GetFloat64(rcv._tab.Pos + flatbuffers.UOffsetT(16))
}
func (rcv *MetricData) Min() float64 {
	return rcv._tab.GetFloat64(rcv._tab.Pos + flatbuffers.UOffsetT(24))
}
func (rcv *MetricData) Max() float64 {
	return rcv._tab.GetFloat64(rcv._tab.Pos + flatbuffers.UOffsetT(32))
}
func (rcv *MetricData) SumSquares() float64 {
	return rcv._tab.GetFloat64(rcv._tab.Pos + flatbuffers.UOffsetT(40))
}
func (rcv *MetricData) Scoped() byte { return rcv._tab.GetByte(rcv._tab.Pos + flatbuffers.UOffsetT(48)) }
func (rcv *MetricData) Forced() byte { return rcv._tab.GetByte(rcv._tab.Pos + flatbuffers.UOffsetT(49)) }

func CreateMetricData(builder *flatbuffers.Builder, count float64, total float64, exclusive float64, min float64, max float64, sumSquares float64, scoped byte, forced byte) flatbuffers.UOffsetT {
	builder.Prep(8, 56)
	builder.Pad(6)
	builder.PrependByte(forced)
	builder.PrependByte(scoped)
	builder.PrependFloat64(sumSquares)
	builder.PrependFloat64(max)
	builder.PrependFloat64(min)
	builder.PrependFloat64(exclusive)
	builder.PrependFloat64(total)
	builder.PrependFloat64(count)
	return builder.Offset()
}
