package main

import (
	"bufio"
	"encoding/hex"
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

const (
	errOpenDevice    = "couldn't open device"
	errGetAppVersion = "couldn't get app version"
	errGetConfig     = "couldn't get configuration"
	errGetAddress    = "couldn't get address"
	errSigningMsg    = "signing error"
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
	fmt.Scanln()
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

	hash := sha3.NewLegacyKeccak256()
	hash.Write([]byte(fmt.Sprintf("\x17Elrond Signed Message:\n%v%s", len(msg), msg)))
	sha := hash.Sum(nil)

	txSingleSigner := &singlesig.Ed25519Signer{}
	_suite := ed25519.NewEd25519()
	keyGen := signing.NewKeyGenerator(_suite)

	_, bytes, _ := bech32.Decode(string(senderAddress))
	bytes, _ = bech32.ConvertBits(bytes, 5, 8, false)
	publicKey, _ := keyGen.PublicKeyFromByteArray(bytes)
	err = txSingleSigner.Verify(publicKey, sha, signature)
	if err != nil {
		log.Println(errSigningMsg, err)
		waitInputAndExit()
	}
	fmt.Println("Signature verified !")

	waitInputAndExit()
}
