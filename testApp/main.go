package main

import (
	"bufio"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"math/big"
	"net/http"
	"os"
	"strconv"
	"strings"

	"./ledger"

	"github.com/btcsuite/btcutil/bech32"
)

const observerHost string = "https://wallet-api.elrond.com"
const isProxy bool = true // if true, send "data" field as string, else send as base64

const (
	noOfShards      = uint32(5)
	strDenomination = "1000000000000000000"
	gasPrice        = uint64(100000000000000)
	gasLimit        = uint64(100000)
	gasPerDataByte  = uint64(1500)
)

const (
	errOpenDevice         = "Couldn't open device"
	errGetAppVersion      = "Couldn't get app version"
	errGetConfig          = "Couldn't get configuration"
	errGetAddress         = "Couldn't get address"
	errGetBalanceAndNonce = "Couldn't get address balance and nonce"
	errEmptyAddress       = "Empty address"
	errInvalidAddress     = "Invalid receiver address"
	errInvalidAmount      = "Invalid ERD amount"
	errSigningTx          = "Signing error"
	errSendingTx          = "Error sending tx"
)

type transactionType struct {
	Nonce     uint64 `json:"nonce"`
	Value     string `json:"value"`
	RcvAddr   string `json:"receiver"`
	SndAddr   string `json:"sender"`
	GasPrice  uint64 `json:"gasPrice,omitempty"`
	GasLimit  uint64 `json:"gasLimit,omitempty"`
	Data      string `json:"data,omitempty"`
	Signature string `json:"signature,omitempty"`
}

type accountType struct {
	Address string `json:"address"`
	Nonce   uint64 `json:"nonce"`
	Balance string `json:"balance"`
}

type accountMessage struct {
	Account accountType `json:"account"`
}

// getSenderInfo returns the balance and nonce of an address
func getSenderInfo(address string) (*big.Int, uint64, error) {
	req, err := http.NewRequest(http.MethodGet,
		fmt.Sprintf("%s/address/%s", observerHost, address), nil)
	if err != nil {
		return nil, 0, err
	}
	client := http.DefaultClient
	resp, err := client.Do(req)
	if err != nil {
		return nil, 0, err
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer resp.Body.Close()
	if err != nil {
		return nil, 0, err
	}
	var balanceMsg accountMessage
	err = json.Unmarshal(body, &balanceMsg)
	if err != nil {
		return nil, 0, err
	}
	balance, _ := big.NewInt(0).SetString(balanceMsg.Account.Balance, 10)

	return balance, balanceMsg.Account.Nonce, nil
}

// getAddressShard returns the assigned shard of an address
func getAddressShard(address string, noOfShards uint32) uint32 {
	n := math.Ceil(math.Log2(float64(noOfShards)))
	var maskHigh, maskLow uint32 = (1 << uint(n)) - 1, (1 << uint(n-1)) - 1
	addressBytes, _ := hex.DecodeString(address)
	addr := uint32(addressBytes[len(addressBytes)-1])
	shard := addr & maskHigh
	if shard > noOfShards-1 {
		shard = addr & maskLow
	}
	return shard
}

func getDeviceInfo(nanos *ledger.NanoS) (network, contractData, account, addressIndex uint8, ledgerVersion string) {
	// retrieving various informations from Ledger
	appVersion, err := nanos.GetVersion()
	if err != nil {
		log.Fatalln(errGetAppVersion, err)
	}
	fmt.Println("Nano S app version: ", appVersion)
	network, contractData, account, addressIndex, _, err = nanos.GetConfiguration()
	if err != nil {
		log.Fatalln(errGetConfig, err)
	}
	strNetwork := "Mainnet"
	if network == 1 {
		strNetwork = "Testnet"
	}
	strContractData := "enabled"
	if contractData == 0 {
		strContractData = "disabled"
	}
	fmt.Printf("Network: %s ; Contract data: %s\n\r", strNetwork, strContractData)
	return
}

func getTxDataFromUser(contractData uint8) (string, *big.Int, string) {
	var err error
	// get tx fields from user
	reader := bufio.NewReader(os.Stdin)
	// read destination address
	fmt.Print("Enter destination address: ")
	strReceiverAddress, _ := reader.ReadString('\n')
	if strReceiverAddress == "" {
		log.Fatalln(errEmptyAddress)
	}
	strReceiverAddress = strReceiverAddress[:len(strReceiverAddress)-1]
	var receiverAddress []byte
	if strings.HasPrefix(strReceiverAddress, "terd") || strings.HasPrefix(strReceiverAddress, "erd") {
		var receiverAddressBech32 []byte
		_, receiverAddressBech32, err = bech32.Decode(strReceiverAddress)
		receiverAddress, _ = bech32.ConvertBits(receiverAddressBech32, 5, 8, false)
	} else {
		receiverAddress, err = hex.DecodeString(strReceiverAddress)
	}
	if err != nil || len(receiverAddress) != 32 {
		log.Fatalln(errInvalidAddress, err)
	}
	strReceiverShard := getAddressShard(hex.EncodeToString(receiverAddress), noOfShards)
	fmt.Printf("Receiver shard: %v\n\r", strReceiverShard)
	// read amount
	fmt.Print("Amount of ERD to send: ")
	strAmount, _ := reader.ReadString('\n')
	if strings.HasSuffix(strAmount, "\n") {
		strAmount = strings.TrimSuffix(strAmount, "\n")
	}
	amount, err := strconv.ParseFloat(strAmount, 64)
	if err != nil {
		log.Fatalln(errInvalidAmount)
	}
	bigFloatAmount := big.NewFloat(0).SetFloat64(amount)
	denomination, _ := big.NewFloat(0).SetString(strDenomination)
	bigFloatAmount.Mul(bigFloatAmount, denomination)
	bigIntAmount := new(big.Int)
	bigFloatAmount.Int(bigIntAmount)
	var data string
	if contractData == 1 {
		// read data field
		fmt.Print("Data field: ")
		data, _ = reader.ReadString('\n')
		if strings.HasSuffix(data, "\n") {
			data = strings.TrimSuffix(data, "\n")
		}
	}
	return strReceiverAddress, bigIntAmount, data
}

func signTransaction(tx *transactionType, nanos *ledger.NanoS) {
	toSign, _ := json.Marshal(&tx)
	fmt.Println("Signing transaction. Please confirm on your Ledger")
	signature, err := nanos.SignTxn(toSign)
	if err != nil {
		log.Fatalln(errSigningTx, err)
	}

	// send transaction
	sigHex := hex.EncodeToString(signature)
	if !isProxy {
		tx.Data = base64.StdEncoding.EncodeToString([]byte(tx.Data))
	}
	tx.Signature = sigHex
}

func broadcastTransaction(tx transactionType) {
	jsonTx, _ := json.Marshal(&tx)
	resp, err := http.Post(fmt.Sprintf("%s/transaction/send", observerHost), "",
		strings.NewReader(string(jsonTx)))
	if err != nil {
		log.Fatalln(errSendingTx, err)
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer resp.Body.Close()
	if err != nil {
		log.Fatalln(errSendingTx, err)
	}
	res := string(body)
	fmt.Printf("Result: %s\n\r", res)
}

// main function
func main() {
	log.SetFlags(0)

	// opening connection with the Ledger device
	var nanos *ledger.NanoS
	nanos, err := ledger.OpenNanoS()
	if err != nil {
		log.Fatalln(errOpenDevice, err)
	}
	_, contractData, account, addressIndex, _ := getDeviceInfo(nanos)

	fmt.Println("Retrieving address. Please confirm on your Ledger")
	address, err := nanos.GetAddress(uint32(account), uint32(addressIndex))
	if err != nil {
		log.Fatalln(errGetAddress, err)
	}
	fmt.Printf("Address: %s\n\r", address)

	// convert sender from bech32 to hex pubkey
	strAddress := string(address)
	_, pubkeyBech32, err := bech32.Decode(strAddress)
	pubkey, _ := bech32.ConvertBits(pubkeyBech32, 5, 8, false)
	strPubkey := hex.EncodeToString(pubkey)
	fmt.Printf("Pubkey: %s\n\r", strPubkey)

	// retrieve sender's nonce and balance
	denomination, _ := big.NewFloat(0).SetString(strDenomination)
	balance, nonce, err := getSenderInfo(strAddress)
	if err != nil || balance == nil {
		log.Fatalln(errGetBalanceAndNonce, err)
	}
	bigFloatBalance, _ := big.NewFloat(0).SetString(balance.String())
	bigFloatBalance.Quo(bigFloatBalance, denomination)
	strBalance := bigFloatBalance.String()
	strSenderShard := getAddressShard(strPubkey, noOfShards)
	fmt.Printf("Sender shard: %v\n\rBalance: %v ERD\n\rNonce: %v\n\r", strSenderShard, strBalance, nonce)

	strReceiverAddress, bigIntAmount, data := getTxDataFromUser(contractData)

	// generate and sign transaction
	var tx transactionType
	tx.SndAddr = strAddress
	tx.RcvAddr = strReceiverAddress
	tx.Value = bigIntAmount.String()
	tx.Nonce = nonce
	tx.GasPrice = gasPrice
	tx.Data = data
	tx.GasLimit = gasLimit + uint64(len(data))*gasPerDataByte

	signTransaction(&tx, nanos)
	broadcastTransaction(tx)
}
