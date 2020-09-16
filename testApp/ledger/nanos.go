package ledger

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"

	"github.com/karalabe/hid"
)

const (
	cla = 0xED // identifies an Elrond app command

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
	Account       uint8
	AddressIndex  uint8
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
	n.Account = resp[1]
	n.AddressIndex = resp[2]
	n.LedgerVersion = fmt.Sprintf("%v.%v.%v", resp[3], resp[4], resp[5])
	return nil
}

// GetAddress retrieves from device the address based on account and address index
func (n *NanoS) GetAddress(account uint32, index uint32) (pubkey []byte, err error) {
	encAccount := make([]byte, 4)
	binary.BigEndian.PutUint32(encAccount, account)
	encIndex := make([]byte, 4)
	binary.BigEndian.PutUint32(encIndex, index)

	resp, err := n.Exchange(cmdGetAddress, p1WithConfirmation, p2DisplayBech32, 8, append(encAccount, encIndex...))
	if err != nil {
		return nil, err
	}
	if int(resp[0]) != len(resp)-1 {
		return nil, errors.New(errBadAddressResponse)
	}
	pubkey = resp[1:]
	return pubkey, nil
}

// SignTxn sends a json marshalized transaction to the device and returns the signature
func (n *NanoS) SignTxn(txData []byte) (sig []byte, err error) {
	buf := new(bytes.Buffer)
	buf.Write(txData)

	var resp []byte = nil
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
		return nil, errors.New(errBadSignature)
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
