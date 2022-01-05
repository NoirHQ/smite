// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
package main

import (
	"fmt"
	"os"

	abci "github.com/tendermint/tendermint/abci/types"
	"github.com/tendermint/tendermint/libs/log"
	nm "github.com/tendermint/tendermint/node"
	"github.com/tendermint/tendermint/proxy"
	"github.com/tendermint/tendermint/libs/service"
	"github.com/tendermint/tendermint/types"
)

import "C"

var (
	node service.Service
)

//export tm_node_start
func tm_node_start() {
	app := NewNoirApp()

	tmNode, err := newTendermint(app)
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

func newTendermint(app abci.Application) (service.Service, error) {
	tmLogger, err := log.NewDefaultLogger(config.LogFormat, config.LogLevel, false)
	if err != nil {
		return nil, err
	}
	logger = tmLogger
	papp := proxy.NewLocalClientCreator(app)
	genDoc, _ := types.GenesisDocFromFile(config.GenesisFile())

	tmNode, err := nm.New(config, logger, papp, genDoc)
	if err != nil {
		return nil, fmt.Errorf("failed to create new Tendermint node: %w", err)
	}
	return tmNode, nil
}
