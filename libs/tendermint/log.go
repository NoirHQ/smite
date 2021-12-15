package main

import (
	"github.com/tendermint/tendermint/libs/log"
)

// typedef const char CConstChar;
import "C"

var (
	logger = log.MustNewDefaultLogger(log.LogFormatPlain, log.LogLevelInfo, false)
)

//export tm_log_info
func tm_log_info(msg *C.CConstChar) {
	logger.Info(C.GoString(msg))
}

//export tm_log_debug
func tm_log_debug(msg *C.CConstChar) {
	logger.Debug(C.GoString(msg))
}

//export tm_log_error
func tm_log_error(msg *C.CConstChar) {
	logger.Error(C.GoString(msg))
}
