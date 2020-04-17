package main

import (
	"bufio"
	"bytes"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math"
	"math/big"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/btcsuite/btcutil/bech32"
	"github.com/karalabe/hid"
)

// DEBUG : if enabled, displays the apdus exchanged between host and device
var DEBUG bool = false

const observerHost string = "https://wallet-api.elrond.com"
const isProxy bool = true // if true, send "data" field as string, else send as base64

const noOfShards uint32 = 5

const (
	// CLA identifies an Elrond app command
	CLA = 0xED

	cmdGetVersion       = 0x01
	cmdGetConfiguration = 0x02
	cmdGetAddress       = 0x03
	cmdSignTxn          = 0x04

	p1WithConfirmation = 0x01
	p1NoConfirmation   = 0x00
	p2DisplayBech32    = 0x00
	p2DisplayHex       = 0x01
)

const (
	codeSuccess              = 0x9000
	codeUserRejected         = 0x6985
	codeInvalidParam         = 0x6b01
	codeInvalidArguments     = 0x6e01
	codeInvalidMessage       = 0x6e02
	codeInvalidP1            = 0x6e03
	codeMessageTooLong       = 0x6e04
	codeReceiverTooLong      = 0x6e05
	codeAmountTooLong        = 0x6e06
	codeContractDataDisabled = 0x6e07
)

var (
	errUserRejected         = errors.New("user denied request")
	errInvalidParam         = errors.New("invalid request parameters")
	errInvalidArguments     = errors.New("Invalid arguments")
	errInvalidMessage       = errors.New("Invalid message")
	errInvalidP1            = errors.New("Invalid P1")
	errMessageTooLong       = errors.New("Message too long")
	errReceiverTooLong      = errors.New("Receiver address too long")
	errAmountTooLong        = errors.New("Amount string too long")
	errContractDataDisabled = errors.New("Contract data is disabled")
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

type balanceType struct {
	Address string `json:"address"`
	Nonce   uint64 `json:"nonce"`
	Balance string `json:"balance"`
}

type balanceMessage struct {
	Account balanceType `json:"account"`
}

type hidFramer struct {
	rw  io.ReadWriter
	seq uint16
	buf [64]byte
	pos int
}

type APDU struct {
	CLA    byte   // 0xED for Elrond
	INS    byte   // Instruction
	P1, P2 byte   // Parameters
	LC     byte   // Data length
	DATA   []byte // Data
}

type apduFramer struct {
	hf  *hidFramer
	buf [2]byte // to read APDU length prefix
}

type NanoS struct {
	device *apduFramer
}

type ErrCode uint16

// Reset resets the communication with the device
func (hf *hidFramer) Reset() {
	hf.seq = 0
}

// Write sends raw data to the device
func (hf *hidFramer) Write(p []byte) (int, error) {
	if DEBUG {
		fmt.Println("HID <=", hex.EncodeToString(p))
	}
	// split into 64-byte chunks
	chunk := make([]byte, 64)
	binary.BigEndian.PutUint16(chunk[:2], 0x0101)
	chunk[2] = 0x05
	var seq uint16
	buf := new(bytes.Buffer)
	binary.Write(buf, binary.BigEndian, uint16(len(p)))
	buf.Write(p)
	for buf.Len() > 0 {
		binary.BigEndian.PutUint16(chunk[3:5], seq)
		n, _ := buf.Read(chunk[5:])
		if n, err := hf.rw.Write(chunk[:5+n]); err != nil {
			return n, err
		}
		seq++
	}
	return len(p), nil
}

// Read reads raw data from the device
func (hf *hidFramer) Read(p []byte) (int, error) {
	if hf.seq > 0 && hf.pos != 64 {
		// drain buf
		n := copy(p, hf.buf[hf.pos:])
		hf.pos += n
		return n, nil
	}
	// read next 64-byte packet
	if n, err := hf.rw.Read(hf.buf[:]); err != nil {
		return 0, err
	} else if n != 64 {
		panic("read less than 64 bytes from HID")
	}
	// parse header
	channelID := binary.BigEndian.Uint16(hf.buf[:2])
	commandTag := hf.buf[2]
	seq := binary.BigEndian.Uint16(hf.buf[3:5])
	if channelID != 0x0101 {
		return 0, fmt.Errorf("bad channel ID 0x%x", channelID)
	} else if commandTag != 0x05 {
		return 0, fmt.Errorf("bad command tag 0x%x", commandTag)
	} else if seq != hf.seq {
		return 0, fmt.Errorf("bad sequence number %v (expected %v)", seq, hf.seq)
	}
	hf.seq++
	// start filling p
	n := copy(p, hf.buf[5:])
	hf.pos = 5 + n
	return n, nil
}

// Exchange sends an APDU to the device and returns the response
func (af *apduFramer) Exchange(apdu APDU) ([]byte, error) {
	if len(apdu.DATA) > 255 {
		panic("APDU data cannot exceed 255 bytes")
	}
	af.hf.Reset()
	data := append([]byte{
		apdu.CLA,
		apdu.INS,
		apdu.P1, apdu.P2,
		apdu.LC,
	}, apdu.DATA...)
	if _, err := af.hf.Write(data); err != nil {
		return nil, err
	}

	// read APDU length
	if _, err := io.ReadFull(af.hf, af.buf[:]); err != nil {
		return nil, err
	}
	// read APDU data
	respLen := binary.BigEndian.Uint16(af.buf[:2])
	resp := make([]byte, respLen)
	_, err := io.ReadFull(af.hf, resp)
	if DEBUG {
		fmt.Println("HID =>", hex.EncodeToString(resp))
	}
	return resp, err
}

// Error formats the error code
func (c ErrCode) Error() string {
	return fmt.Sprintf("Error code 0x%x", uint16(c))
}

// Exchange sends a command to the device and returns the response
func (n *NanoS) Exchange(cmd byte, p1, p2, lc byte, data []byte) (resp []byte, err error) {
	resp, err = n.device.Exchange(APDU{
		CLA:  CLA,
		INS:  cmd,
		P1:   p1,
		P2:   p2,
		LC:   lc,
		DATA: data,
	})
	if err != nil {
		return nil, err
	} else if len(resp) < 2 {
		return nil, errors.New("APDU response missing status code")
	}
	code := binary.BigEndian.Uint16(resp[len(resp)-2:])
	resp = resp[:len(resp)-2]
	switch code {
	case codeSuccess:
		err = nil
	case codeUserRejected:
		err = errUserRejected
	case codeInvalidParam:
		err = errInvalidParam
	case codeInvalidArguments:
		err = errInvalidArguments
	case codeInvalidMessage:
		err = errInvalidMessage
	case codeInvalidP1:
		err = errInvalidP1
	case codeMessageTooLong:
		err = errMessageTooLong
	case codeReceiverTooLong:
		err = errReceiverTooLong
	case codeAmountTooLong:
		err = errAmountTooLong
	case codeContractDataDisabled:
		err = errContractDataDisabled
	default:
		err = ErrCode(code)
	}
	return
}

// GetVersion retrieves from device the app version
func (n *NanoS) GetVersion() (version string, err error) {
	resp, err := n.Exchange(cmdGetVersion, 0, 0, 0, nil)
	if err != nil {
		return "", err
	}
	return string(resp), nil
}

// GetConfiguration retrieves from device its configuration
func (n *NanoS) GetConfiguration() (network, contractData, account, addressIndex uint8, ledgerVersion string, err error) {
	resp, err := n.Exchange(cmdGetConfiguration, 0, 0, 0, nil)
	if err != nil {
		return
	}
	if len(resp) != 7 {
		err = errors.New("GetConfiguration erroneous response")
		return
	}
	network = resp[0]
	contractData = resp[1]
	account = resp[2]
	addressIndex = resp[3]
	ledgerVersion = fmt.Sprintf("%v.%v.%v", resp[4], resp[5], resp[6])
	return
}

// GetAddress retrieves from device the address based on account and address index
func (n *NanoS) GetAddress(account uint32, index uint32) (pubkey []byte, err error) {
	encAccount := make([]byte, 4)
	binary.LittleEndian.PutUint32(encAccount, account)
	encIndex := make([]byte, 4)
	binary.LittleEndian.PutUint32(encIndex, index)

	resp, err := n.Exchange(cmdGetAddress, p1WithConfirmation, p2DisplayBech32, 8, append(encAccount, encIndex...))
	if err != nil {
		return nil, err
	}
	if int(resp[0]) != len(resp)-1 {
		return nil, errors.New("Invalid get address response")
	}
	pubkey = resp[1:]
	return pubkey, nil
}

// SignTxn sends a json marshalized transaction to the device and returns the signature
func (n *NanoS) SignTxn(txData []byte, sigIndex uint16, keyIndex uint32) (sig []byte, err error) {
	buf := new(bytes.Buffer)
	buf.Write(txData)

	var resp []byte
	for buf.Len() > 0 {
		var p1 byte = 0x80
		if resp == nil {
			p1 = 0x00
		}
		toSend := buf.Next(255)
		resp, err = n.Exchange(cmdSignTxn, p1, 0, byte(len(toSend)), toSend)
		if err != nil {
			return nil, err
		}
	}
	if len(resp) != 65 || resp[0] != 64 {
		return nil, errors.New("Invalid signature received from Ledger")
	}
	sig = make([]byte, 64)
	copy(sig[:], resp[1:])
	return
}

// OpenNanoS establishes the connection to the device
func OpenNanoS() (*NanoS, error) {
	const (
		ledgerVendorID       = 0x2c97
		ledgerNanoSProductID = 0x1015
	)

	// search for Nano S
	devices := hid.Enumerate(ledgerVendorID, ledgerNanoSProductID)
	if len(devices) == 0 {
		return nil, errors.New("Nano S not detected")
	}

	// open the device
	device, err := devices[0].Open()
	if err != nil {
		return nil, err
	}

	// wrap raw device I/O in HID+APDU protocols
	return &NanoS{
		device: &apduFramer{
			hf: &hidFramer{
				rw: device,
			},
		},
	}, nil
}

// main function
func main() {
	log.SetFlags(0)

	// opening connection with the Ledger device
	var nanos *NanoS
	nanos, err := OpenNanoS()
	if err != nil {
		log.Fatalln("Couldn't open device: ", err)
	}
	// retrieving various informations from Ledger
	appVersion, err := nanos.GetVersion()
	if err != nil {
		log.Fatalln("Couldn't get app version:", err)
	}
	fmt.Println("Nano S app version: ", appVersion)
	network, contractData, account, addressIndex, _, err := nanos.GetConfiguration()
	if err != nil {
		log.Fatalln("Couldn't get configuration: ", err)
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
	fmt.Println("Retrieving address. Please confirm on your Ledger")
	address, err := nanos.GetAddress(uint32(account), uint32(addressIndex))
	if err != nil {
		log.Fatalln("Couldn't get address: ", err)
	}
	fmt.Printf("Address: %s\n\r", address)

	// convert sender from bech32 to hex pubkey
	strAddress := string(address)
	_, pubkeyBech32, err := bech32.Decode(strAddress)
	pubkey, _ := bech32.ConvertBits(pubkeyBech32, 5, 8, false)
	strPubkey := hex.EncodeToString(pubkey)
	fmt.Printf("Pubkey: %s\n\r", strPubkey)

	// retrieve sender's nonce and balance
	denomination, _ := big.NewFloat(0).SetString("1000000000000000000")
	balance, nonce, err := getSenderInfo(strAddress)
	if err != nil || balance == nil {
		log.Fatalln("Couldn't get address balance and nonce", err)
	}
	bigFloatBalance, _ := big.NewFloat(0).SetString(balance.String())
	bigFloatBalance.Quo(bigFloatBalance, denomination)
	strBalance := bigFloatBalance.String()
	strSenderShard := getAddressShard(strPubkey, noOfShards)
	fmt.Printf("Sender shard: %v\n\rBalance: %v ERD\n\rNonce: %v\n\r", strSenderShard, strBalance, nonce)

	// get tx fields from user
	reader := bufio.NewReader(os.Stdin)
	// read destination address
	fmt.Print("Enter destination address: ")
	strReceiverAddress, _ := reader.ReadString('\n')
	if strReceiverAddress == "" {
		log.Fatalln("Empty receiver address")
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
		log.Fatalln("Invalid receiver address: ", err)
	}
	strReceiverShard := getAddressShard(hex.EncodeToString(receiverAddress), noOfShards)
	fmt.Printf("Receiver shard: %v\n\r", strReceiverShard)
	if strSenderShard == strReceiverShard {
		fmt.Println("Intra-shard transaction")
	} else {
		fmt.Println("Cross-shard transaction")
	}
	// read amount
	fmt.Print("Amount of ERD to send: ")
	strAmount, _ := reader.ReadString('\n')
	if strings.HasSuffix(strAmount, "\n") {
		strAmount = strings.TrimSuffix(strAmount, "\n")
	}
	amount, err := strconv.ParseFloat(strAmount, 64)
	if err != nil {
		log.Fatalln("Invalid ERD amount")
	}
	bigFloatAmount := big.NewFloat(0).SetFloat64(amount)
	bigFloatAmount.Mul(bigFloatAmount, denomination)
	bigIntAmount := new(big.Int)
	bigFloatAmount.Int(bigIntAmount)
	if err != nil {
		log.Fatalln("Invalid ERD amount: ", err)
	}
	var data string
	if contractData == 1 {
		// read data field
		fmt.Print("Data field: ")
		data, _ = reader.ReadString('\n')
		if strings.HasSuffix(data, "\n") {
			data = strings.TrimSuffix(data, "\n")
		}
	}

	// generate and sign transaction
	var tx transactionType
	tx.SndAddr = strAddress
	tx.RcvAddr = strReceiverAddress
	tx.Value = bigIntAmount.String()
	tx.Nonce = nonce
	tx.GasPrice = 100000000000000
	tx.Data = base64.StdEncoding.EncodeToString([]byte(data))
	tx.GasLimit = 100000 + uint64(len(data))*1500

	toSign, _ := json.Marshal(&tx)
	fmt.Println("Signing transaction. Please confirm on your Ledger")
	signature, err := nanos.SignTxn(toSign, 0, 0)
	if err != nil {
		log.Fatalln("Signing error: ", err)
	}

	// send transaction
	sigHex := hex.EncodeToString(signature)
	if isProxy {
		tx.Data = data
	}
	tx.Signature = sigHex
	jsonTx, _ := json.Marshal(&tx)
	resp, err := http.Post(fmt.Sprintf("%s/transaction/send", observerHost), "",
		strings.NewReader(string(jsonTx)))
	if err != nil {
		log.Fatalln("Error sending tx: ", err)
	}
	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		log.Fatalln("Error sending tx: ", err)
	}
	res := string(body)
	fmt.Printf("Result: %s\n\r", res)
	resp.Body.Close()
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
	if err != nil {
		return nil, 0, err
	}
	var balanceMsg balanceMessage
	json.Unmarshal(body, &balanceMsg)
	resp.Body.Close()
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
