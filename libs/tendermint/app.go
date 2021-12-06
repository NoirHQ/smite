package main

import (
	"encoding/hex"
	"fmt"
	"unsafe"

	abci "github.com/tendermint/tendermint/abci/types"
)

// #cgo darwin LDFLAGS: -Wl,-undefined,dynamic_lookup -flat_namespace
// #cgo linux LDFLAGS: -Wl,-undefined,dynamic_lookup
// #include <tendermint/abci.h>
import "C"

type NoirApp struct {
	//todo - directly map pointers to app?
}

var _ abci.Application = (*NoirApp)(nil)

func NewNoirApp() *NoirApp {
	return &NoirApp{
	}
}

func (NoirApp) PrepareProposal(req abci.RequestPrepareProposal) abci.ResponsePrepareProposal {
	return abci.ResponsePrepareProposal{BlockData: req.BlockData}
}

func (NoirApp) ExtendVote(req abci.RequestExtendVote) abci.ResponseExtendVote {
	return abci.ResponseExtendVote{}
}

func (NoirApp) VerifyVoteExtension(req abci.RequestVerifyVoteExtension) abci.ResponseVerifyVoteExtension {
	return abci.ResponseVerifyVoteExtension{}
}

func (NoirApp) Info(req abci.RequestInfo) abci.ResponseInfo {
	info := C.info()
	if info != nil {
		defer C.free(unsafe.Pointer(info))
		last_block_app_hash, err := hex.DecodeString(C.GoString(info.last_block_app_hash))
		if err != nil {
			panic(err)
		}
		logger.Info(fmt.Sprintf(" *** Info: last_block_height=%v, last_block_app_hash=%v", int64(info.last_block_height), C.GoString(info.last_block_app_hash)))
		return abci.ResponseInfo{LastBlockHeight: int64(info.last_block_height), LastBlockAppHash: last_block_app_hash}
	}
	return abci.ResponseInfo{}
}

func (NoirApp) DeliverTx(req abci.RequestDeliverTx) abci.ResponseDeliverTx {
	C.deliver_tx(C.CBytes(req.Tx), C.uint(len(req.Tx)))
	return abci.ResponseDeliverTx{Code: 0}
}

func (NoirApp) CheckTx(req abci.RequestCheckTx) abci.ResponseCheckTx {
	C.check_tx(C.CBytes(req.Tx), C.uint(len(req.Tx)))
	//bytes := C.GoBytes(ptr, C.int(len(req.Tx)))
	//C.check_tx(bytes)
	logger.Info(fmt.Sprintf(" *** CheckTx: tx=%v, len=%v, type=%v", req.Tx, len(req.Tx), req.Type))
	return abci.ResponseCheckTx{Code: 0, Sender: "sender_id", Priority: int64(10)}
}

func (NoirApp) Commit() abci.ResponseCommit {
	cApphash := C.commit()
	if cApphash != nil {
		defer C.free(unsafe.Pointer(cApphash)) // frees memory allocated in C
		apphash := C.GoString(cApphash)
		apphashBytes, err := hex.DecodeString(apphash)
		if err != nil {
			panic(err)
		}
		logger.Info(fmt.Sprintf(" *** Commit: apphash=%v", apphash))
		return abci.ResponseCommit{Data: apphashBytes}
	}
	return abci.ResponseCommit{}
}

func (NoirApp) Query(req abci.RequestQuery) abci.ResponseQuery {
	return abci.ResponseQuery{Code: 0}
}

func (NoirApp) InitChain(req abci.RequestInitChain) abci.ResponseInitChain {
	return abci.ResponseInitChain{}
}

func (NoirApp) BeginBlock(req abci.RequestBeginBlock) abci.ResponseBeginBlock {
	C.begin_block(C.int64_t(req.Header.Height))
	return abci.ResponseBeginBlock{}
}

func (NoirApp) EndBlock(req abci.RequestEndBlock) abci.ResponseEndBlock {
	C.end_block()
	return abci.ResponseEndBlock{}
}

func (NoirApp) ListSnapshots(abci.RequestListSnapshots) abci.ResponseListSnapshots {
	return abci.ResponseListSnapshots{}
}

func (NoirApp) OfferSnapshot(abci.RequestOfferSnapshot) abci.ResponseOfferSnapshot {
	return abci.ResponseOfferSnapshot{}
}

func (NoirApp) LoadSnapshotChunk(abci.RequestLoadSnapshotChunk) abci.ResponseLoadSnapshotChunk {
	return abci.ResponseLoadSnapshotChunk{}
}

func (NoirApp) ApplySnapshotChunk(abci.RequestApplySnapshotChunk) abci.ResponseApplySnapshotChunk {
	return abci.ResponseApplySnapshotChunk{}
}
