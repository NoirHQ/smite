package main

import (
	"fmt"
	"path/filepath"
	"time"

	"github.com/spf13/viper"

	cfg "github.com/tendermint/tendermint/config"
)

// typedef const char CConstChar;
import "C"

var (
	config = cfg.DefaultConfig()
	ctxTimeout = 4 * time.Second
)

//export tm_config_load
func tm_config_load(config_file *C.CConstChar) C.char {
	configFile := C.GoString(config_file)
	homeDir := filepath.Dir(filepath.Dir(configFile))
	config.SetRoot(homeDir)
	cfg.EnsureRoot(homeDir)
	viper.Set("home", homeDir)
	viper.SetConfigName("config")
	viper.AddConfigPath(homeDir)
	viper.AddConfigPath(filepath.Join(homeDir, "config"))
	if err := viper.ReadInConfig(); err == nil {
	} else if _, ok := err.(viper.ConfigFileNotFoundError); !ok {
		fmt.Printf("viper failed to read config file: %w\n", err)
		return C.char(0)
	}
	if err := viper.Unmarshal(config); err != nil {
		fmt.Printf("viper failed to unmarshal config: %w\n", err)
		return C.char(0)
	}
	if err := config.ValidateBasic(); err != nil {
		fmt.Printf("config is invalid: %w\n", err)
		return C.char(0)
	}
	return C.char(1)
}

//export tm_config_save
func tm_config_save() C.char {
	if err := initFilesWithConfig(config); err != nil {
		return C.char(0)
	}
	return C.char(1)
}

//export tm_config_set
func tm_config_set(key *C.CConstChar, value *C.CConstChar) {
	viper.Set(C.GoString(key), C.GoString(value))
}
