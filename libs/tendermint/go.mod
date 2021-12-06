module github.com/noir-protocol/noir/tendermint

go 1.16

require (
	github.com/spf13/viper v1.8.1
	github.com/tendermint/tendermint v0.35.0
)

replace github.com/tendermint/tendermint v0.35.0 => github.com/noir-protocol/tendermint v0.35.0-dev.0.20211116122348-53565cb815e2
