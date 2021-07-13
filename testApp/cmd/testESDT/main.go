package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"log"
	"math"
	"math/big"

	"github.com/ElrondNetwork/ledger-elrond/testApp/cmd/common"
	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
	"github.com/btcsuite/btcd/btcec"
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

	var data string
	strReceiverAddress, bigIntAmount, _, err := common.GetTxDataFromUser(0, denomination, ticker)
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

	// ticker len, ticker, id_len, id, decimals, chain_id_len, chain_id, signature
	toHashStr := fmt.Sprintf("%c%s%c%s%c%c%s", 3, "DRD", 20, "4452442d633462303861", 2, 1, "T")
	h := sha256.New()
	h.Write([]byte(toHashStr))
	hash := h.Sum(nil)

	privateKey, publicKey := btcec.PrivKeyFromBytes(btcec.S256(), common.TestSkBytes)
	signature, _ := privateKey.Sign(hash)
	fmt.Printf("private key: %s \n", hex.EncodeToString(privateKey.Serialize()))
	fmt.Printf("public key: %s \n", hex.EncodeToString(publicKey.SerializeUncompressed()))
	fmt.Printf("signature: %s \n", hex.EncodeToString(signature.Serialize()))
	toSend := append([]byte(toHashStr), signature.Serialize()...)

	err = nanos.ProvideESDTInfo(toSend)
	if err != nil {
		log.Println(err)
		common.WaitInputAndExit()
	}

	data = "ESDTTransfer@4452442d633462303861@7B"

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
	tx.Version = common.TxHashSignVersion
	tx.Options = common.TxHashSignOptions

	err = common.SignTransactionHash(&tx, nanos)
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
