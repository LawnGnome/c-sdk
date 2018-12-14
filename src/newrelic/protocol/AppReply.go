// automatically generated, do not modify

package protocol

import (
	flatbuffers "github.com/google/flatbuffers/go"
)

type AppReply struct {
	_tab flatbuffers.Table
}

func (rcv *AppReply) Init(buf []byte, i flatbuffers.UOffsetT) {
	rcv._tab.Bytes = buf
	rcv._tab.Pos = i
}

func (rcv *AppReply) Status() int8 {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(4))
	if o != 0 {
		return rcv._tab.GetInt8(o + rcv._tab.Pos)
	}
	return 0
}

func (rcv *AppReply) ConnectReply() []byte {
	o := flatbuffers.UOffsetT(rcv._tab.Offset(6))
	if o != 0 {
		return rcv._tab.ByteVector(o + rcv._tab.Pos)
	}
	return nil
}

func AppReplyStart(builder *flatbuffers.Builder) { builder.StartObject(2) }
func AppReplyAddStatus(builder *flatbuffers.Builder, status int8) {
	builder.PrependInt8Slot(0, status, 0)
}
func AppReplyAddConnectReply(builder *flatbuffers.Builder, connectReply flatbuffers.UOffsetT) {
	builder.PrependUOffsetTSlot(1, flatbuffers.UOffsetT(connectReply), 0)
}
func AppReplyEnd(builder *flatbuffers.Builder) flatbuffers.UOffsetT { return builder.EndObject() }
