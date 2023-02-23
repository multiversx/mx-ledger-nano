package common

import (
	"bufio"
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

const (
	ProxyHost          = "https://testnet-gateway.multiversx.com"
	Hrp                = "erd"
	MainnetId          = "1"
	SignMessagePrepend = "\x17Elrond Signed Message:\n"
	TickerMainnet      = "eGLD"
	TickerTestnet      = "XeGLD"

	TxHashSignVersion = 2
	TxHashSignOptions = 1
)

var Status = [...]string{"Disabled", "Enabled"}

/*
For testing the ESDT tokens whitelisting, one can use the following sk. For this secret key, you have to update the public
key LEDGER_SIGNATURE_PUBLIC_KEY in ledger-elrond/src/provide_ESDT_info.h
The public key specific to this secret key is:
static const uint8_t LEDGER_SIGNATURE_PUBLIC_KEY[] = {
    0x04, 0x0d, 0x04, 0x9d, 0xd5, 0x3d, 0x97, 0x7d, 0x21, 0x92, 0xed, 0xeb, 0xba, 0xac, 0x71,
    0x39, 0x20, 0xda, 0xd2, 0x95, 0x59, 0x9a, 0x09, 0xf0, 0x8c, 0xe6, 0x25, 0x33, 0x37, 0x99,
    0x37, 0x5f, 0xc2, 0x81, 0xda, 0xf0, 0x24, 0x09, 0x66, 0x01, 0x34, 0xd2, 0x98, 0x8c, 0x4f,
    0xd3, 0x52, 0x6e, 0xde, 0x39, 0x4f, 0xa0, 0xe5, 0xdc, 0x3d, 0x3f, 0xb7, 0x30, 0xeb, 0x53,
    0xab, 0x35, 0xa6, 0x57, 0x5c};
*/
var TestSkBytes = []byte{0xb5, 0xe3, 0xcf, 0xb4, 0x42, 0xb0, 0xac, 0xad, 0xf5, 0xf5, 0xd1, 0xed, 0x1f, 0x7a, 0xc7, 0xb6,
	0xff, 0xc3, 0x28, 0xab, 0x29, 0x7b, 0xa0, 0xeb, 0x0e, 0x5c, 0xe7, 0x33, 0x8d, 0xe5, 0x49, 0x5e}

const (
	ErrOpenDevice                        = "couldn't open device"
	ErrGetAppVersion                     = "couldn't get app version"
	ErrGetConfig                         = "couldn't get configuration"
	ErrSetAddress                        = "couldn't set account and address index"
	ErrGetAddress                        = "couldn't get address"
	ErrGetNetworkConfig                  = "couldn't get network config"
	ErrGetBalanceAndNonce                = "couldn't get address balance and nonce"
	ErrEmptyAddress                      = "empty address"
	ErrInvalidAddress                    = "invalid receiver address"
	ErrInvalidAmount                     = "invalid eGLD amount"
	ErrSigningTx                         = "signing error"
	ErrSendingTx                         = "error sending tx"
	ErrInvalidBalanceString              = "invalid balance string"
	ErrInvalidHRP                        = "invalid bech32 hrp"
	ErrGetAddressShard                   = "getAddressShard error"
	ErrGetAccountAndAddressIndexFromUser = "invalid account or address index provided by user"
	ErrSigningMsg                        = "signing error"
)

type NetworkConfig struct {
	Data struct {
		Config struct {
			ChainID                  string `json:"erd_chain_id"`
			Denomination             int    `json:"erd_denomination"`
			GasPerDataByte           uint64 `json:"erd_gas_per_data_byte"`
			LatestTagSoftwareVersion string `json:"erd_latest_tag_software_version"`
			MetaConsensusGroupSize   uint32 `json:"erd_meta_consensus_group_size"`
			MinGasLimit              uint64 `json:"erd_min_gas_limit"`
			MinGasPrice              uint64 `json:"erd_min_gas_price"`
			MinTransactionVersion    uint32 `json:"erd_min_transaction_version"`
			NumMetachainNodes        uint32 `json:"erd_num_metachain_nodes"`
			NumNodesInShard          uint32 `json:"erd_num_nodes_in_shard"`
			NumShardsWithoutMeta     uint32 `json:"erd_num_shards_without_meta"`
			RoundDuration            uint32 `json:"erd_round_duration"`
			ShardConsensusGroupSize  uint32 `json:"erd_shard_consensus_group_size"`
			StartTime                uint32 `json:"erd_start_time"`
		} `json:"config"`
	} `json:"data"`
}

type Transaction struct {
	Nonce            uint64 `json:"nonce"`
	Value            string `json:"value"`
	RcvAddr          string `json:"receiver"`
	SndAddr          string `json:"sender"`
	SenderUsername   []byte `json:"senderUsername,omitempty"`
	ReceiverUsername []byte `json:"receiverUsername,omitempty"`
	GasPrice         uint64 `json:"gasPrice,omitempty"`
	GasLimit         uint64 `json:"gasLimit,omitempty"`
	Data             []byte `json:"data,omitempty"`
	Signature        string `json:"signature,omitempty"`
	ChainID          string `json:"chainID"`
	Version          uint32 `json:"version"`
	Options          uint32 `json:"options,omitempty"`
}

type GetAccountResponse struct {
	Data struct {
		Account struct {
			Address string `json:"address"`
			Nonce   uint64 `json:"nonce"`
			Balance string `json:"balance"`
		} `json:"account"`
	} `json:"data"`
}

// GetSenderInfo returns the balance and nonce of an address
func GetSenderInfo(address string) (*big.Int, uint64, error) {
	req, err := http.NewRequest(http.MethodGet,
		fmt.Sprintf("%s/address/%s", ProxyHost, address), nil)
	if err != nil {
		return nil, 0, err
	}
	client := http.DefaultClient
	resp, err := client.Do(req)
	if err != nil {
		return nil, 0, err
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer func() {
		_ = resp.Body.Close()
	}()
	if err != nil {
		return nil, 0, err
	}
	var accInfo GetAccountResponse
	err = json.Unmarshal(body, &accInfo)
	if err != nil {
		return nil, 0, err
	}
	balance, ok := big.NewInt(0).SetString(accInfo.Data.Account.Balance, 10)
	if !ok {
		return nil, 0, errors.New(ErrInvalidBalanceString)
	}

	return balance, accInfo.Data.Account.Nonce, nil
}

// GetAddressShard returns the assigned shard of an address
func GetAddressShard(bech32Address string, noOfShards uint32) (uint32, error) {
	// convert sender from bech32 to hex pubkey
	h, pubkeyBech32, err := bech32.Decode(bech32Address)
	if err != nil {
		return 0, err
	}
	if h != Hrp {
		return 0, errors.New(ErrInvalidHRP)
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

// GetNetworkConfig reads the network config from the proxy and returns a networkConfig object
func GetNetworkConfig() (*NetworkConfig, error) {
	req, err := http.NewRequest(http.MethodGet, fmt.Sprintf("%s/network/config", ProxyHost), nil)
	if err != nil {
		return nil, err
	}
	client := http.DefaultClient
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer func() {
		_ = resp.Body.Close()
	}()
	if err != nil {
		return nil, err
	}
	netConfig := &NetworkConfig{}
	err = json.Unmarshal(body, netConfig)
	if err != nil {
		return nil, err
	}
	return netConfig, nil
}

// GetDeviceInfo retrieves various information from Ledger
func GetDeviceInfo(nanos *ledger.NanoS) error {
	err := nanos.GetVersion()
	if err != nil {
		log.Println(ErrGetAppVersion)
		return err
	}
	err = nanos.GetConfiguration()
	if err != nil {
		log.Println(ErrGetConfig)
		return err
	}
	return nil
}

// GetTxDataFromUser retrieves tx fields from user
func GetTxDataFromUser(contractData uint8, denomination *big.Float, ticker string) (string, *big.Int, string, error) {
	var err error
	reader := bufio.NewReader(os.Stdin)
	// read destination address
	fmt.Print("Enter destination address: ")
	strReceiverAddress, _ := reader.ReadString('\n')
	if strReceiverAddress == "" {
		log.Println(ErrEmptyAddress)
		return "", nil, "", err
	}
	strReceiverAddress = strings.TrimSpace(strReceiverAddress)
	_, _, err = bech32.Decode(strReceiverAddress)
	if err != nil {
		log.Println(ErrInvalidAddress)
		return "", nil, "", err
	}

	// read amount
	fmt.Printf("Amount of %s to send: ", ticker)
	strAmount, _ := reader.ReadString('\n')
	strAmount = strings.TrimSpace(strAmount)
	bigFloatAmount, ok := big.NewFloat(0).SetPrec(0).SetString(strAmount)
	if !ok {
		log.Println(ErrInvalidAmount)
		return "", nil, "", err
	}
	bigFloatAmount.Mul(bigFloatAmount, denomination)
	bigIntAmount := new(big.Int)
	bigFloatAmount.Int(bigIntAmount)
	var data string
	if contractData == 1 {
		// read data field
		fmt.Print("Data field: ")
		data, _ = reader.ReadString('\n')
		data = strings.TrimSpace(data)
	}
	return strReceiverAddress, bigIntAmount, data, nil
}

// SignTransactionHash sends the tx to Ledger for user confirmation and signing
func SignTransactionHash(tx *Transaction, nanos *ledger.NanoS) error {
	toSign, err := json.Marshal(tx)
	if err != nil {
		return err
	}
	fmt.Println("Signing transaction. Please confirm on your Ledger")
	fmt.Println("\nTransaction payload to be signed:")
	fmt.Println(string(toSign))
	signature, err := nanos.SignTxHash(toSign)
	if err != nil {
		log.Println(ErrSigningTx)
		return err
	}

	sigHex := hex.EncodeToString(signature)
	tx.Signature = sigHex

	fullTx, err := json.Marshal(tx)
	if err != nil {
		return err
	}
	fmt.Println("\nTransaction that will be sent:")
	fmt.Println(string(fullTx))
	return nil
}

// SignTransaction sends the tx to Ledger for user confirmation and signing
func SignTransaction(tx *Transaction, nanos *ledger.NanoS) error {
	toSign, err := json.Marshal(tx)
	if err != nil {
		return err
	}
	fmt.Println("Signing transaction. Please confirm on your Ledger")
	signature, err := nanos.SignTx(toSign)
	if err != nil {
		log.Println(ErrSigningTx)
		return err
	}

	sigHex := hex.EncodeToString(signature)
	tx.Signature = sigHex
	return nil
}

// BroadcastTransaction broadcasts the transaction in the network
func BroadcastTransaction(tx Transaction) error {
	jsonTx, _ := json.Marshal(&tx)
	resp, err := http.Post(fmt.Sprintf("%s/transaction/send", ProxyHost), "",
		strings.NewReader(string(jsonTx)))
	if err != nil {
		log.Println(ErrSendingTx)
		return err
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer func() {
		_ = resp.Body.Close()
	}()
	if err != nil {
		log.Println(ErrSendingTx)
		return err
	}
	res := string(body)
	fmt.Printf("Result: %s\n\r", res)
	return nil
}

// GetAccountAndAddressIndexFromUser retrieves the account and address index from user
func GetAccountAndAddressIndexFromUser() (uint32, uint32, error) {
	reader := bufio.NewReader(os.Stdin)
	fmt.Print("Account: ")
	strAccount, _ := reader.ReadString('\n')
	strAccount = strings.TrimSpace(strAccount)
	account, err := strconv.ParseUint(strAccount, 10, 32)
	if err != nil {
		return 0, 0, err
	}
	fmt.Print("Address index: ")
	strAddressIndex, _ := reader.ReadString('\n')
	strAddressIndex = strings.TrimSpace(strAddressIndex)
	addressIndex, err := strconv.ParseUint(strAddressIndex, 10, 32)
	if err != nil {
		return 0, 0, err
	}
	return uint32(account), uint32(addressIndex), nil
}

func WaitInputAndExit() {
	fmt.Println("Press enter to continue...")
	_, _ = fmt.Scanln()
	os.Exit(1)
}
