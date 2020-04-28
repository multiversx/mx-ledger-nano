package main

import (
	"bufio"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"math/big"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
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
	hrpMainnet      = "erd"
	hrpTestnet      = "xerd"
)

var network = [...]string{"Mainnet", "Testnet"}
var status = [...]string{"Disabled", "Enabled"}

const (
	errOpenDevice           = "couldn't open device"
	errGetAppVersion        = "couldn't get app version"
	errGetConfig            = "couldn't get configuration"
	errGetAddress           = "couldn't get address"
	errGetBalanceAndNonce   = "couldn't get address balance and nonce"
	errEmptyAddress         = "empty address"
	errInvalidAddress       = "invalid receiver address"
	errInvalidAmount        = "invalid ERD amount"
	errSigningTx            = "signing error"
	errSendingTx            = "error sending tx"
	errInvalidBalanceString = "invalid balance string"
	errInvalidHRP           = "invalid bech32 hrp"
	errGetAddressShard      = "getAddressShard error"
)

type transaction struct {
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
	balance, ok := big.NewInt(0).SetString(balanceMsg.Account.Balance, 10)
	if !ok {
		return nil, 0, errors.New(errInvalidBalanceString)
	}

	return balance, balanceMsg.Account.Nonce, nil
}

// getAddressShard returns the assigned shard of an address
func getAddressShard(bech32Address string, noOfShards uint32) (uint32, error) {
	// convert sender from bech32 to hex pubkey
	hrp, pubkeyBech32, err := bech32.Decode(bech32Address)
	if err != nil {
		return 0, err
	}
	if hrp != hrpMainnet && hrp != hrpTestnet {
		return 0, errors.New(errInvalidHRP)
	}
	pubkey, err := bech32.ConvertBits(pubkeyBech32, 5, 8, false)
	if err != nil {
		return 0, err
	}
	address := hex.EncodeToString(pubkey)

	n := math.Ceil(math.Log2(float64(noOfShards)))
	var maskHigh, maskLow uint32 = (1 << uint(n)) - 1, (1 << uint(n-1)) - 1
	addressBytes, err := hex.DecodeString(address)
	if err != nil {
		return 0, err
	}
	addr := uint32(addressBytes[len(addressBytes)-1])
	shard := addr & maskHigh
	if shard > noOfShards-1 {
		shard = addr & maskLow
	}
	return shard, nil
}

// getDeviceInfo retrieves various informations from Ledger
func getDeviceInfo(nanos *ledger.NanoS) error {
	err := nanos.GetVersion()
	if err != nil {
		log.Println(errGetAppVersion)
		return err
	}
	err = nanos.GetConfiguration()
	if err != nil {
		log.Println(errGetConfig)
		return err
	}
	return nil
}

// getTxDataFromUser retrieves tx fields from user
func getTxDataFromUser(contractData uint8) (string, *big.Int, string, error) {
	var err error
	reader := bufio.NewReader(os.Stdin)
	// read destination address
	fmt.Print("Enter destination address: ")
	strReceiverAddress, _ := reader.ReadString('\n')
	if strReceiverAddress == "" {
		log.Println(errEmptyAddress)
		return "", nil, "", err
	}
	if strings.HasSuffix(strReceiverAddress, "\n") {
		strReceiverAddress = strings.TrimSuffix(strReceiverAddress, "\n")
	}
	_, _, err = bech32.Decode(strReceiverAddress)
	if err != nil {
		log.Println(errInvalidAddress)
		return "", nil, "", err
	}

	// read amount
	fmt.Print("Amount of ERD to send: ")
	strAmount, _ := reader.ReadString('\n')
	if strings.HasSuffix(strAmount, "\n") {
		strAmount = strings.TrimSuffix(strAmount, "\n")
	}
	amount, err := strconv.ParseFloat(strAmount, 64)
	if err != nil {
		log.Println(errInvalidAmount)
		return "", nil, "", err
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
	return strReceiverAddress, bigIntAmount, data, nil
}

// signTransaction sends the tx to Ledger for user confirmation and signing
func signTransaction(tx *transaction, nanos *ledger.NanoS) error {
	toSign, err := json.Marshal(tx)
	if err != nil {
		return err
	}
	fmt.Println("Signing transaction. Please confirm on your Ledger")
	signature, err := nanos.SignTxn(toSign)
	if err != nil {
		log.Println(errSigningTx)
		return err
	}

	sigHex := hex.EncodeToString(signature)
	if !isProxy {
		tx.Data = base64.StdEncoding.EncodeToString([]byte(tx.Data))
	}
	tx.Signature = sigHex
	return nil
}

// broadcastTransaction broadcasts the transaction in the network
func broadcastTransaction(tx transaction) error {
	jsonTx, _ := json.Marshal(&tx)
	resp, err := http.Post(fmt.Sprintf("%s/transaction/send", observerHost), "",
		strings.NewReader(string(jsonTx)))
	if err != nil {
		log.Println(errSendingTx)
		return err
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer resp.Body.Close()
	if err != nil {
		log.Println(errSendingTx)
		return err
	}
	res := string(body)
	fmt.Printf("Result: %s\n\r", res)
	return nil
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
	err = getDeviceInfo(nanos)
	if err != nil {
		log.Fatalln(err)
	}
	fmt.Println("Nano S app version: ", nanos.AppVersion)
	fmt.Printf("Network: %s ; Contract data: %s\n\r", network[nanos.Network], status[nanos.ContractData])

	fmt.Println("Retrieving address. Please confirm on your Ledger")
	senderAddress, err := nanos.GetAddress(uint32(nanos.Account), uint32(nanos.AddressIndex))
	if err != nil {
		log.Fatalln(errGetAddress, err)
	}
	fmt.Printf("Address: %s\n\r", senderAddress)

	// retrieve sender's nonce and balance
	denomination, _ := big.NewFloat(0).SetString(strDenomination)
	balance, nonce, err := getSenderInfo(string(senderAddress))
	if err != nil || balance == nil {
		log.Fatalln(errGetBalanceAndNonce, err)
	}
	bigFloatBalance, _ := big.NewFloat(0).SetString(balance.String())
	bigFloatBalance.Quo(bigFloatBalance, denomination)
	strBalance := bigFloatBalance.String()
	strSenderShard, err := getAddressShard(string(senderAddress), noOfShards)
	if err != nil {
		log.Fatalln(errGetAddressShard, err)
	}
	fmt.Printf("Sender shard: %v\n\rBalance: %v ERD\n\rNonce: %v\n\r", strSenderShard, strBalance, nonce)

	strReceiverAddress, bigIntAmount, data, err := getTxDataFromUser(nanos.ContractData)
	if err != nil {
		log.Fatalln(err)
	}
	strReceiverShard, err := getAddressShard(strReceiverAddress, noOfShards)
	if err != nil {
		log.Fatalln(errGetAddressShard, err)
	}
	fmt.Printf("Receiver shard: %v\n\r", strReceiverShard)

	// generate and sign transaction
	var tx transaction
	tx.SndAddr = string(senderAddress)
	tx.RcvAddr = strReceiverAddress
	tx.Value = bigIntAmount.String()
	tx.Nonce = nonce
	tx.GasPrice = gasPrice
	tx.Data = data
	tx.GasLimit = gasLimit + uint64(len(data))*gasPerDataByte

	err = signTransaction(&tx, nanos)
	if err != nil {
		log.Fatalln(err)
	}
	err = broadcastTransaction(tx)
	if err != nil {
		log.Fatalln(err)
	}
}
