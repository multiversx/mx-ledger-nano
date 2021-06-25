package ledger

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"math"

	"github.com/karalabe/hid"
)

const (
	cla = 0xED // identifies an Elrond app command

	cmdGetVersion       = 0x01
	cmdGetConfiguration = 0x02
	cmdGetAddress       = 0x03
	cmdSignTxn          = 0x04
	cmdSetAddress       = 0x05
	cmdSignMsg          = 0x06
	cmdSignTxnHash      = 0x07
	cmdProvideESDTInfo  = 0x08

	p1WithConfirmation = 0x01
	p1NoConfirmation   = 0x00
	p2DisplayBech32    = 0x00
	p2DisplayHex       = 0x01
	p1First            = 0x00
	p1More             = 0x80
)

const (
	codeSuccess              = 0x9000
	codeUserRejected         = 0x6985
	codeUnknownInstruction   = 0x6d00
	codeWrongCLA             = 0x6e00
	codeInvalidArguments     = 0x6e01
	codeInvalidMessage       = 0x6e02
	codeInvalidP1            = 0x6e03
	codeMessageTooLong       = 0x6e04
	codeReceiverTooLong      = 0x6e05
	codeAmountTooLong        = 0x6e06
	codeContractDataDisabled = 0x6e07
	codeMessageIncomplete    = 0x6e08
	codeWrongTxVersion       = 0x6e09
	codeNonceTooLong         = 0x6e0a
	codeInvalidAmount        = 0x6e0b
	codeInvalidFee           = 0x6e0c
	codePrettyFailed         = 0x6e0d
	codeDataTooLong          = 0x6e0e
)

const (
	errBadConfigResponse  = "GetConfiguration erroneous response"
	errBadAddressResponse = "Invalid get address response"
	errBadSignature       = "Invalid signature received from Ledger"
	errNotDetected        = "Nano S not detected"
)

const sigLen = 64

var (
	errUserRejected         = errors.New("user denied request")
	errUnknownInstruction   = errors.New("unknown instruction (INS)")
	errWrongCLA             = errors.New("wrong CLA")
	errInvalidArguments     = errors.New("invalid arguments")
	errInvalidMessage       = errors.New("invalid message")
	errInvalidP1            = errors.New("invalid P1")
	errMessageTooLong       = errors.New("message too long")
	errReceiverTooLong      = errors.New("receiver address too long")
	errAmountTooLong        = errors.New("amount string too long")
	errContractDataDisabled = errors.New("contract data is disabled")
	errWrongTxVersion       = errors.New("wrong tx version")
	errNonceTooLong         = errors.New("nonce too long")
	errInvalidAmount        = errors.New("invalid amount")
	errInvalidFee           = errors.New("invalid fee")
	errPrettyFailed         = errors.New("failed to make the amount look pretty")
	errDataTooLong          = errors.New("data too long")
)

type NanoS struct {
	ContractData  uint8
	Account       uint32
	AddressIndex  uint32
	AppVersion    string
	LedgerVersion string
	device        *apduFramer
}

// Exchange sends a command to the device and returns the response
func (n *NanoS) Exchange(cmd byte, p1, p2, lc byte, data []byte) (resp []byte, err error) {
	resp, err = n.device.Exchange(APDU{
		CLA:  cla,
		INS:  cmd,
		P1:   p1,
		P2:   p2,
		LC:   lc,
		DATA: data,
	})
	if err != nil {
		return nil, err
	} else if len(resp) < 2 {
		return nil, errors.New(errMissingStatusCode)
	}
	code := binary.BigEndian.Uint16(resp[len(resp)-2:])
	resp = resp[:len(resp)-2]
	switch code {
	case codeSuccess:
		err = nil
	case codeUserRejected:
		err = errUserRejected
	case codeUnknownInstruction:
		err = errUnknownInstruction
	case codeWrongCLA:
		err = errWrongCLA
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
	case codeWrongTxVersion:
		err = errWrongTxVersion
	case codeNonceTooLong:
		err = errNonceTooLong
	case codeInvalidAmount:
		err = errInvalidAmount
	case codeInvalidFee:
		err = errInvalidFee
	case codePrettyFailed:
		err = errPrettyFailed
	case codeDataTooLong:
		err = errDataTooLong
	default:
		err = fmt.Errorf("Error code 0x%x", code)
	}
	return
}

// GetVersion retrieves from device the app version
func (n *NanoS) GetVersion() error {
	resp, err := n.Exchange(cmdGetVersion, 0, 0, 0, nil)
	if err != nil {
		return err
	}
	n.AppVersion = string(resp)
	return nil
}

// GetConfiguration retrieves from device its configuration
func (n *NanoS) GetConfiguration() error {
	resp, err := n.Exchange(cmdGetConfiguration, 0, 0, 0, nil)
	if err != nil {
		return err
	}
	if len(resp) != 6 {
		return errors.New(errBadConfigResponse)
	}
	n.ContractData = resp[0]
	// To emphasize that these fields are not to be taken into account anymore
	// since now those variables are 32 bit long, but we still expect 6 bytes
	// transmitted to maintain compatibility with the web wallet. The respective
	// values (account and address_index) should now be read with the help of the getAddress function.
	//n.Account = resp[1]
	//n.AddressIndex = resp[2]
	n.LedgerVersion = fmt.Sprintf("%v.%v.%v", resp[3], resp[4], resp[5])
	return nil
}

// GetAddress retrieves from device the address based on account and address index
func (n *NanoS) GetAddress(account uint32, index uint32) (pubkey []byte, err error) {
	return n.getAddress(account, index, p1WithConfirmation)
}

// GetAddressWithoutConfirmation retrieves from device the address based on account and address index
// (without confirmation on device)
func (n *NanoS) GetAddressWithoutConfirmation(account uint32, index uint32) (pubkey []byte, err error) {
	return n.getAddress(account, index, p1NoConfirmation)
}

func (n *NanoS) getAddress(account uint32, index uint32, confirmation byte) (pubkey []byte, err error) {
	encAccount := make([]byte, 4)
	binary.BigEndian.PutUint32(encAccount, account)
	encIndex := make([]byte, 4)
	binary.BigEndian.PutUint32(encIndex, index)

	resp, err := n.Exchange(cmdGetAddress, confirmation, p2DisplayBech32, 8, append(encAccount, encIndex...))
	if err != nil {
		return nil, err
	}
	if int(resp[0]) != len(resp)-1 {
		return nil, errors.New(errBadAddressResponse)
	}
	pubkey = resp[1:]
	return pubkey, nil
}

// SetAddress sets the account and address index
func (n *NanoS) SetAddress(account uint32, index uint32) error {
	encAccount := make([]byte, 4)
	binary.BigEndian.PutUint32(encAccount, account)
	encIndex := make([]byte, 4)
	binary.BigEndian.PutUint32(encIndex, index)

	_, err := n.Exchange(cmdSetAddress, 0, 0, 8, append(encAccount, encIndex...))
	return err
}

// SignTx sends a json marshalized transaction to the device and returns the signature
func (n *NanoS) SignTx(txData []byte) (sig []byte, err error) {
	return n.sign(txData, cmdSignTxn)
}

func (n *NanoS) sign(txData []byte, cmd byte) (sig []byte, err error) {
	buf := new(bytes.Buffer)
	buf.Write(txData)

	var resp []byte = nil
	for buf.Len() > 0 {
		var p1 byte = p1More
		if resp == nil {
			p1 = p1First
		}
		toSend := buf.Next(math.MaxUint8)
		resp, err = n.Exchange(cmd, p1, 0, byte(len(toSend)), toSend)
		if err != nil {
			return nil, err
		}
	}
	if len(resp) != sigLen+1 || resp[0] != byte(sigLen) {
		return nil, errors.New(errBadSignature)
	}
	sig = make([]byte, sigLen)
	copy(sig[:], resp[1:])
	return
}

// SignMsg sends a message to the device and returns the signature
func (n *NanoS) SignMsg(msg string) (sig []byte, err error) {
	buf := new(bytes.Buffer)
	encLen := make([]byte, 4)
	binary.BigEndian.PutUint32(encLen, uint32(len(msg)))
	buf.Write(encLen)
	buf.Write([]byte(msg))
	return n.sign(buf.Bytes(), cmdSignMsg)
}

// SignTxHash sends a transaction hash to the device and returns the signature
func (n *NanoS) SignTxHash(txData []byte) (sig []byte, err error) {
	return n.sign(txData, cmdSignTxnHash)
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
		return nil, errors.New(errNotDetected)
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

func (n *NanoS) ProvideESDTInfo(info []byte) error {
	_, err := n.Exchange(cmdProvideESDTInfo, 0, 0, byte(len(info)), info)
	return err
}
