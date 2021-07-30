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
	"github.com/ElrondNetwork/ledger-elrond/testApp/cmd/common"
	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
	"github.com/btcsuite/btcutil/bech32"
	"golang.org/x/crypto/sha3"
)

func checkSignature(address string, message string, signature []byte) error {
	hash := sha3.NewLegacyKeccak256()
	hash.Write([]byte(fmt.Sprintf("%s%v%s", common.SignMessagePrepend, len(message), message)))
	sha := hash.Sum(nil)

	txSingleSigner := &singlesig.Ed25519Signer{}
	suite := ed25519.NewEd25519()
	keyGen := signing.NewKeyGenerator(suite)

	hrp, bytes, err := bech32.Decode(address)
	if err != nil {
		return err
	}
	if hrp != "erd" {
		return errors.New(common.ErrInvalidAddress)
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
		log.Println(common.ErrOpenDevice, err)
		common.WaitInputAndExit()
	}
	err = common.GetDeviceInfo(nanos)
	if err != nil {
		log.Println(err)
		common.WaitInputAndExit()
	}
	fmt.Println("Nano S app version: ", nanos.AppVersion)

	nanos.Account, nanos.AddressIndex, err = common.GetAccountAndAddressIndexFromUser()
	if err != nil {
		log.Println(common.ErrGetAccountAndAddressIndexFromUser, err)
		common.WaitInputAndExit()
	}

	err = nanos.SetAddress(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(common.ErrSetAddress, err)
		common.WaitInputAndExit()
	}

	senderAddress, err := nanos.GetAddressWithoutConfirmation(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(common.ErrGetAddress, err)
		common.WaitInputAndExit()
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
		log.Println(common.ErrSigningMsg, err)
		common.WaitInputAndExit()
	}

	sigHex := hex.EncodeToString(signature)
	fmt.Printf("Signature: %s\n\r", sigHex)

	err = checkSignature(string(senderAddress), msg, signature)
	if err != nil {
		log.Println(common.ErrSigningMsg, err)
		common.WaitInputAndExit()
	}
	fmt.Println("Signature verified !")

	common.WaitInputAndExit()
}
