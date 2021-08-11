package main

import (
	"bufio"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/ElrondNetwork/ledger-elrond/testApp/cmd/common"
	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
)

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
	fmt.Printf("Contract data: %s\n\r", common.Status[nanos.ContractData])

	nanos.Account, nanos.AddressIndex, err = common.GetAccountAndAddressIndexFromUser()
	if err != nil {
		log.Println(common.ErrGetAccountAndAddressIndexFromUser, err)
		common.WaitInputAndExit()
	}

	fmt.Println("Retrieving address. Please confirm on your Ledger")
	err = nanos.SetAddress(nanos.Account, nanos.AddressIndex)
	if err != nil {
		log.Println(common.ErrSetAddress, err)
		common.WaitInputAndExit()
	}

	fmt.Print("Token to be signed: ")
	reader := bufio.NewReader(os.Stdin)
	tkn, _ := reader.ReadString('\n')
	tkn = strings.TrimSpace(tkn)

	senderAddress, signature, err := nanos.GetAddressWithAuthToken(nanos.Account, nanos.AddressIndex, tkn)
	if err != nil {
		log.Println(common.ErrGetAddress, err)
		common.WaitInputAndExit()
	}
	fmt.Printf("Address: %s\nSignature: %s\n\r", senderAddress, signature)
}
