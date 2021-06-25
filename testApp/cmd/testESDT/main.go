package main

import (
	"bufio"
	"crypto/sha256"
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
	"github.com/decred/dcrd/dcrec/secp256k1"
	// "github.com/ethereum/go-ethereum/crypto/secp256k1"
)

const proxyHost string = "https://testnet-gateway.elrond.com"

const (
	hrp       = "erd"
	mainnetId = "1"

	tx_hash_sign_version = 2
	tx_hash_sign_options = 1
)

var ticker = "eGLD"
var status = [...]string{"Disabled", "Enabled"}
var denomination *big.Float
var pkBytes = []byte{0xb5, 0xe3, 0xcf, 0xb4, 0x42, 0xb0, 0xac, 0xad, 0xf5, 0xf5, 0xd1, 0xed, 0x1f, 0x7a, 0xc7, 0xb6,
	0xff, 0xc3, 0x28, 0xab, 0x29, 0x7b, 0xa0, 0xeb, 0x0e, 0x5c, 0xe7, 0x33, 0x8d, 0xe5, 0x49, 0x5e}

const (
	errOpenDevice                        = "couldn't open device"
	errGetAppVersion                     = "couldn't get app version"
	errGetConfig                         = "couldn't get configuration"
	errSetAddress                        = "couldn't set account and address index"
	errGetAddress                        = "couldn't get address"
	errGetNetworkConfig                  = "couldn't get network config"
	errGetBalanceAndNonce                = "couldn't get address balance and nonce"
	errEmptyAddress                      = "empty address"
	errInvalidAddress                    = "invalid receiver address"
	errInvalidAmount                     = "invalid eGLD amount"
	errSigningTx                         = "signing error"
	errSendingTx                         = "error sending tx"
	errInvalidBalanceString              = "invalid balance string"
	errInvalidHRP                        = "invalid bech32 hrp"
	errGetAddressShard                   = "getAddressShard error"
	errGetAccountAndAddressIndexFromUser = "invalid account or address index provided by user"
)

type networkConfig struct {
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

type transaction struct {
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

type getAccountResponse struct {
	Data struct {
		Account struct {
			Address string `json:"address"`
			Nonce   uint64 `json:"nonce"`
			Balance string `json:"balance"`
		} `json:"account"`
	} `json:"data"`
}

// getSenderInfo returns the balance and nonce of an address
func getSenderInfo(address string) (*big.Int, uint64, error) {
	req, err := http.NewRequest(http.MethodGet,
		fmt.Sprintf("%s/address/%s", proxyHost, address), nil)
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
	var accInfo getAccountResponse
	err = json.Unmarshal(body, &accInfo)
	if err != nil {
		return nil, 0, err
	}
	balance, ok := big.NewInt(0).SetString(accInfo.Data.Account.Balance, 10)
	if !ok {
		return nil, 0, errors.New(errInvalidBalanceString)
	}

	return balance, accInfo.Data.Account.Nonce, nil
}

// getAddressShard returns the assigned shard of an address
func getAddressShard(bech32Address string, noOfShards uint32) (uint32, error) {
	// convert sender from bech32 to hex pubkey
	h, pubkeyBech32, err := bech32.Decode(bech32Address)
	if err != nil {
		return 0, err
	}
	if h != hrp {
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

// getNetworkConfig reads the network config from the proxy and returns a networkConfig object
func getNetworkConfig() (*networkConfig, error) {
	req, err := http.NewRequest(http.MethodGet, fmt.Sprintf("%s/network/config", proxyHost), nil)
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
	netConfig := &networkConfig{}
	err = json.Unmarshal(body, netConfig)
	if err != nil {
		return nil, err
	}
	return netConfig, nil
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
	strReceiverAddress = strings.TrimSpace(strReceiverAddress)
	_, _, err = bech32.Decode(strReceiverAddress)
	if err != nil {
		log.Println(errInvalidAddress)
		return "", nil, "", err
	}

	// read amount
	fmt.Printf("Amount of %s to send: ", ticker)
	strAmount, _ := reader.ReadString('\n')
	strAmount = strings.TrimSpace(strAmount)
	bigFloatAmount, ok := big.NewFloat(0).SetPrec(0).SetString(strAmount)
	if !ok {
		log.Println(errInvalidAmount)
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

// signTransaction sends the tx to Ledger for user confirmation and signing
func signTransaction(tx *transaction, nanos *ledger.NanoS) error {
	toSign, err := json.Marshal(tx)
	if err != nil {
		return err
	}
	fmt.Println("Signing transaction. Please confirm on your Ledger")
	fmt.Println("\nTransaction payload to be signed:")
	fmt.Println(string(toSign))
	signature, err := nanos.SignTxHash(toSign)
	if err != nil {
		log.Println(errSigningTx)
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

// broadcastTransaction broadcasts the transaction in the network
func broadcastTransaction(tx transaction) error {
	jsonTx, _ := json.Marshal(&tx)
	resp, err := http.Post(fmt.Sprintf("%s/transaction/send", proxyHost), "",
		strings.NewReader(string(jsonTx)))
	if err != nil {
		log.Println(errSendingTx)
		return err
	}
	body, err := ioutil.ReadAll(resp.Body)
	defer func() {
		_ = resp.Body.Close()
	}()
	if err != nil {
		log.Println(errSendingTx)
		return err
	}
	res := string(body)
	fmt.Printf("Result: %s\n\r", res)
	return nil
}

// getAccountAndAddressIndexFromUser retrieves the account and address index from user
func getAccountAndAddressIndexFromUser() (uint32, uint32, error) {
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

func waitInputAndExit() {
	fmt.Println("Press enter to continue...")
	_, _ = fmt.Scanln()
	os.Exit(1)
}

// main function
func main() {
	log.SetFlags(0)

	// opening connection with the Ledger device
	var nanos *ledger.NanoS
	nanos, err := ledger.OpenNanoS()
	if err != nil {
		log.Println(errOpenDevice, err)
		waitInputAndExit()
	}
	err = getDeviceInfo(nanos)
	if err != nil {
		log.Println(err)
		waitInputAndExit()
	}
	fmt.Println("Nano S app version: ", nanos.AppVersion)
	fmt.Printf("Contract data: %s\n\r", status[nanos.ContractData])

	netConfig, err := getNetworkConfig()
	if err != nil {
		log.Println(errGetNetworkConfig, err)
		waitInputAndExit()
	}
	fmt.Printf("Chain ID: %s\n\rTx version: %v\n\r",
		netConfig.Data.Config.ChainID, netConfig.Data.Config.MinTransactionVersion)
	if netConfig.Data.Config.ChainID != mainnetId {
		ticker = "XeGLD"
	}

	nanos.Account, nanos.AddressIndex, err = getAccountAndAddressIndexFromUser()
	if err != nil {
		log.Println(errGetAccountAndAddressIndexFromUser, err)
		waitInputAndExit()
	}

	fmt.Println("Retrieving address. Please confirm on your Ledger")
	err = nanos.SetAddress(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(errSetAddress, err)
		waitInputAndExit()
	}
	senderAddress, err := nanos.GetAddress(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(errGetAddress, err)
		waitInputAndExit()
	}
	fmt.Printf("Address: %s\n\r", senderAddress)

	// retrieve sender's nonce and balance
	denomination = big.NewFloat(math.Pow10(netConfig.Data.Config.Denomination))
	balance, nonce, err := getSenderInfo(string(senderAddress))
	if err != nil || balance == nil {
		log.Println(errGetBalanceAndNonce, err)
		waitInputAndExit()
	}
	bigFloatBalance, _ := big.NewFloat(0).SetString(balance.String())
	bigFloatBalance.Quo(bigFloatBalance, denomination)
	strBalance := bigFloatBalance.String()
	strSenderShard, err := getAddressShard(string(senderAddress), netConfig.Data.Config.NumShardsWithoutMeta)
	if err != nil {
		log.Println(errGetAddressShard, err)
		waitInputAndExit()
	}
	fmt.Printf("Sender shard: %v\n\rBalance: %v %s\n\rNonce: %v\n\r", strSenderShard, strBalance, ticker, nonce)

	var data string
	strReceiverAddress, bigIntAmount, _, err := getTxDataFromUser(0)
	if err != nil {
		log.Println(err)
		waitInputAndExit()
	}
	strReceiverShard, err := getAddressShard(strReceiverAddress, netConfig.Data.Config.NumShardsWithoutMeta)
	if err != nil {
		log.Println(errGetAddressShard, err)
		waitInputAndExit()
	}
	fmt.Printf("Receiver shard: %v\n\r", strReceiverShard)

	// ticker len, ticker, id_len, id, decimals, chain_id_len, chain_id, signature
	toHashStr := fmt.Sprintf("%c%s%c%s%c%c%s", 3, "DRD", 20, "4452442d633462303861", 2, 1, "T")
	h := sha256.New()
	h.Write([]byte(toHashStr))
	hash := h.Sum(nil)

	privateKey, _ := secp256k1.PrivKeyFromBytes(pkBytes)
	signature, _ := privateKey.Sign(hash)
	fmt.Println(hex.EncodeToString(signature.Serialize())) // debug
	toSend := append([]byte(toHashStr), signature.Serialize()...)

	err = nanos.ProvideESDTInfo(toSend)
	if err != nil {
		log.Println(err)
		waitInputAndExit()
	}

	data = "ESDTTransfer@4452442d633462303861@7B"

	// generate and sign transaction
	var tx transaction
	tx.SndAddr = string(senderAddress)
	tx.RcvAddr = strReceiverAddress
	tx.Value = bigIntAmount.String()
	tx.Nonce = nonce
	tx.GasPrice = netConfig.Data.Config.MinGasPrice
	tx.Data = []byte(data)
	tx.GasLimit = netConfig.Data.Config.MinGasLimit + uint64(len(data))*netConfig.Data.Config.GasPerDataByte
	tx.ChainID = netConfig.Data.Config.ChainID
	tx.Version = tx_hash_sign_version
	tx.Options = tx_hash_sign_options

	err = signTransaction(&tx, nanos)
	if err != nil {
		log.Println(err)
		waitInputAndExit()
	}
	err = broadcastTransaction(tx)
	if err != nil {
		log.Println(err)
	}
	waitInputAndExit()
}
