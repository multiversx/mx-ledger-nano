package main

import (
	"fmt"
	"log"

	"github.com/ElrondNetwork/ledger-elrond/testApp/cmd/common"
	"github.com/ElrondNetwork/ledger-elrond/testApp/ledger"
)

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
	senderAddress, signature, err := nanos.GetAddressWithAuthToken(nanos.Account, nanos.AddressIndex, "test token 123 {}")
	if err != nil {
		log.Println(common.ErrGetAddress, err)
		common.WaitInputAndExit()
	}
	fmt.Printf("Address: %s\nSignature: %s\n\r", senderAddress, signature)
}
