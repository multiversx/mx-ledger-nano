package main

import (
	"bufio"
	"encoding/hex"
	"errors"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/ElrondNetwork/elrond-go/crypto/signing"
	"github.com/ElrondNetwork/elrond-go/crypto/signing/ed25519"
	"github.com/ElrondNetwork/elrond-go/crypto/signing/ed25519/singlesig"
	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
	"github.com/btcsuite/btcutil/bech32"
	"golang.org/x/crypto/sha3"
)

const prepend = "\x17Elrond Signed Message:\n"

const (
	errOpenDevice     = "couldn't open device"
	errGetAppVersion  = "couldn't get app version"
	errGetConfig      = "couldn't get configuration"
	errGetAddress     = "couldn't get address"
	errSigningMsg     = "signing error"
	errInvalidAddress = "not an eGLD address. it should start with 'erd'"
)

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

func waitInputAndExit() {
	fmt.Println("Press enter to continue...")
	_, _ = fmt.Scanln()
	os.Exit(1)
}

func checkSignature(address string, message string, signature []byte) error {
	hash := sha3.NewLegacyKeccak256()
	hash.Write([]byte(fmt.Sprintf("%s%v%s", prepend, len(message), message)))
	sha := hash.Sum(nil)

	txSingleSigner := &singlesig.Ed25519Signer{}
	suite := ed25519.NewEd25519()
	keyGen := signing.NewKeyGenerator(suite)

	hrp, bytes, err := bech32.Decode(address)
	if err != nil {
		return err
	}
	if hrp != "erd" {
		return errors.New(errInvalidAddress)
	}
	bytes, err = bech32.ConvertBits(bytes, 5, 8, false)
	if err != nil {
		return err
	}
	publicKey, err := keyGen.PublicKeyFromByteArray(bytes)
	if err != nil {
		return err
	}
	return txSingleSigner.Verify(publicKey, sha, signature)
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

	senderAddress, err := nanos.GetAddressWithoutConfirmation(uint32(nanos.Account), uint32(nanos.AddressIndex))
	if err != nil {
		log.Println(errGetAddress, err)
		waitInputAndExit()
	}
	fmt.Printf("Address: %s\n\r", senderAddress)

	// read message
	fmt.Print("Message to be signed: ")
	reader := bufio.NewReader(os.Stdin)
	msg, _ := reader.ReadString('\n')
	msg = strings.TrimSpace(msg)

	// sign message
	signature, err := nanos.SignMsg(msg)
	if err != nil {
		log.Println(errSigningMsg, err)
		waitInputAndExit()
	}

	sigHex := hex.EncodeToString(signature)
	fmt.Printf("Signature: %s\n\r", sigHex)

	err = checkSignature(string(senderAddress), msg, signature)
	if err != nil {
		log.Println(errSigningMsg, err)
		waitInputAndExit()
	}
	fmt.Println("Signature verified !")

	waitInputAndExit()
}
