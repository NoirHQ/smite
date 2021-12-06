package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"

	"github.com/spf13/viper"

	abci "github.com/tendermint/tendermint/abci/types"
	cfg "github.com/tendermint/tendermint/config"
	"github.com/tendermint/tendermint/libs/log"
	"github.com/tendermint/tendermint/libs/service"
	nm "github.com/tendermint/tendermint/node"
	"github.com/tendermint/tendermint/proxy"
	"github.com/tendermint/tendermint/types"
)

// typedef const char CConstChar;
import "C"

var (
	configFile string
	node service.Service
	logger = log.MustNewDefaultLogger(log.LogFormatPlain, log.LogLevelInfo, false)
)

func init() {
	flag.StringVar(&configFile, "config", filepath.Join(os.Getenv("HOME"), ".tendermint/config/config.toml"), "Path to config.toml")
}

func main() {
}

//export tm_node_start
func tm_node_start() {
	app := NewNoirApp()

	tmNode, err := newTendermint(app, configFile)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%v", err)
		os.Exit(2)
	}
	node = tmNode

	node.Start()
}

//export tm_node_stop
func tm_node_stop() {
	node.Stop()
	node.Wait()
}

func newTendermint(app abci.Application, configFile string) (service.Service, error) {
	// read config
	config := cfg.DefaultConfig()
	config.SetRoot(filepath.Dir(filepath.Dir(configFile)))
	viper.SetConfigFile(configFile)
	if err := viper.ReadInConfig(); err != nil {
		return nil, fmt.Errorf("viper failed to read config file: %w", err)
	}
	if err := viper.Unmarshal(config); err != nil {
		return nil, fmt.Errorf("viper failed to unmarshal config: %w", err)
	}
	if err := config.ValidateBasic(); err != nil {
		return nil, fmt.Errorf("config is invalid: %w", err)
	}

	// create logger
	logger, err := log.NewDefaultLogger(config.LogFormat, config.LogLevel, false)
	if err != nil {
		return nil, err
	}

	// read private validator
	papp := proxy.NewLocalClientCreator(app)

	// read from genesis file
	genDoc, _ := types.GenesisDocFromFile(config.GenesisFile())

	// create node
	tmNode, err := nm.New(config, logger, papp, genDoc)
	if err != nil {
		return nil, fmt.Errorf("failed to create new Tendermint node: %w", err)
	}
	return tmNode, nil
}

//export tm_ilog
func tm_ilog(s *C.CConstChar) {
	logger.Info(C.GoString(s))
}

//export tm_dlog
func tm_dlog(s *C.CConstChar) {
	logger.Debug(C.GoString(s))
}

//export tm_elog
func tm_elog(s *C.CConstChar) {
	logger.Error(C.GoString(s))
}
