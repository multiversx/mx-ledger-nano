package main

import (
	"fmt"
	"log"
	"math"
	"math/big"

	"github.com/ElrondNetwork/ledger-elrond/testApp/cmd/common"
	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
)

// main function
func main() {
	log.SetFlags(0)

	// opening connection with the Ledger device
	var nanos *ledger.NanoS

	nanos, err := ledger.OpenNanoS()
	if err != nil {
		log.Println(common.ErrOpenDevice, err)
		common.WaitInputAndExit()
	}
	err = common.GetDeviceInfo(nanos)
	if err != nil {
		log.Println(err)
		common.WaitInputAndExit()
	}
	fmt.Println("Nano S app version: ", nanos.AppVersion)
	fmt.Printf("Contract data: %s\n\r", common.Status[nanos.ContractData])
	fmt.Println("WARNING: regular transaction signing is deprecated since v1.0.11 so this app won't work " +
		"with applications newer than this.")

	netConfig, err := common.GetNetworkConfig()
	if err != nil {
		log.Println(common.ErrGetNetworkConfig, err)
		common.WaitInputAndExit()
	}
	fmt.Printf("Chain ID: %s\n\rTx version: %v\n\r",
		netConfig.Data.Config.ChainID, netConfig.Data.Config.MinTransactionVersion)

	ticker := common.TickerMainnet
	if netConfig.Data.Config.ChainID != common.MainnetId {
		ticker = common.TickerTestnet
	}

	nanos.Account, nanos.AddressIndex, err = common.GetAccountAndAddressIndexFromUser()
	if err != nil {
		log.Println(common.ErrGetAccountAndAddressIndexFromUser, err)
		common.WaitInputAndExit()
	}

	fmt.Println("Retrieving address. Please confirm on your Ledger")
	err = nanos.SetAddress(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(common.ErrSetAddress, err)
		common.WaitInputAndExit()
	}
	senderAddress, err := nanos.GetAddress(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(common.ErrGetAddress, err)
		common.WaitInputAndExit()
	}
	fmt.Printf("Address: %s\n\r", senderAddress)

	// retrieve sender's nonce and balance
	denomination := big.NewFloat(math.Pow10(netConfig.Data.Config.Denomination))
	balance, nonce, err := common.GetSenderInfo(string(senderAddress))
	if err != nil || balance == nil {
		log.Println(common.ErrGetBalanceAndNonce, err)
		common.WaitInputAndExit()
	}
	bigFloatBalance, _ := big.NewFloat(0).SetString(balance.String())
	bigFloatBalance.Quo(bigFloatBalance, denomination)
	strBalance := bigFloatBalance.String()
	strSenderShard, err := common.GetAddressShard(string(senderAddress), netConfig.Data.Config.NumShardsWithoutMeta)
	if err != nil {
		log.Println(common.ErrGetAddressShard, err)
		common.WaitInputAndExit()
	}
	fmt.Printf("Sender shard: %v\n\rBalance: %v %s\n\rNonce: %v\n\r", strSenderShard, strBalance, ticker, nonce)

	strReceiverAddress, bigIntAmount, data, err := common.GetTxDataFromUser(nanos.ContractData, denomination, ticker)
	if err != nil {
		log.Println(err)
		common.WaitInputAndExit()
	}
	strReceiverShard, err := common.GetAddressShard(strReceiverAddress, netConfig.Data.Config.NumShardsWithoutMeta)
	if err != nil {
		log.Println(common.ErrGetAddressShard, err)
		common.WaitInputAndExit()
	}
	fmt.Printf("Receiver shard: %v\n\r", strReceiverShard)

	// generate and sign transaction
	var tx common.Transaction
	tx.SndAddr = string(senderAddress)
	tx.RcvAddr = strReceiverAddress
	tx.Value = bigIntAmount.String()
	tx.Nonce = nonce
	tx.GasPrice = netConfig.Data.Config.MinGasPrice
	tx.Data = []byte(data)
	tx.GasLimit = netConfig.Data.Config.MinGasLimit + uint64(len(data))*netConfig.Data.Config.GasPerDataByte
	tx.ChainID = netConfig.Data.Config.ChainID
	tx.Version = netConfig.Data.Config.MinTransactionVersion

	err = common.SignTransaction(&tx, nanos)
	if err != nil {
		log.Println(err)
		common.WaitInputAndExit()
	}
	err = common.BroadcastTransaction(tx)
	if err != nil {
		log.Println(err)
	}
	common.WaitInputAndExit()
}
