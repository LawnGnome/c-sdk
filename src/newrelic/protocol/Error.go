// automatically generated, do not modify

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type Error struct {
	_tab flatbuffers.Table
}

func (rcv *Error) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *Error) Priority() int32 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetInt32(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *Error) Data() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func ErrorStart(builder *flatbuffers.Builder) { builder.StartObject(2) }
func ErrorAddPriority(builder *flatbuffers.Builder, priority int32) {
	builder.PrependInt32Slot(0, priority, 0)
}
func ErrorAddData(builder *flatbuffers.Builder, data flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(1, flatbuffers.UOffsetT(data), 0)
}
func ErrorEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT { return builder.EndObject() }
