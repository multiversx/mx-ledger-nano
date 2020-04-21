package ledger

import (
	"bytes"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"io"
)

const (
	errHidMessageTooShort = "read less than 64 bytes from HID"
	errBadChannelID       = "bad channel ID 0x%x"
	errBadCommandTag      = "bad command tag 0x%x"
	errBadSequenceNumber  = "bad sequence number %v (expected %v)"
	errApduTooLong        = "APDU data cannot exceed 255 bytes"
	errMissingStatusCode  = "APDU response missing status code"
)

// debug : if enabled, displays the apdus exchanged between host and device
var debug bool = false

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

// Reset resets the communication with the device
func (hf *hidFramer) Reset() {
	hf.seq = 0
}

// Write sends raw data to the device
func (hf *hidFramer) Write(p []byte) (int, error) {
	if debug {
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
		panic(errHidMessageTooShort)
	}
	// parse header
	channelID := binary.BigEndian.Uint16(hf.buf[:2])
	commandTag := hf.buf[2]
	seq := binary.BigEndian.Uint16(hf.buf[3:5])
	if channelID != 0x0101 {
		return 0, fmt.Errorf(errBadChannelID, channelID)
	} else if commandTag != 0x05 {
		return 0, fmt.Errorf(errBadCommandTag, commandTag)
	} else if seq != hf.seq {
		return 0, fmt.Errorf(errBadSequenceNumber, seq, hf.seq)
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
		panic(errApduTooLong)
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
	if debug {
		fmt.Println("HID =>", hex.EncodeToString(resp))
	}
	return resp, err
}
