# Elrond app for the Ledger Nano S

## Introduction

This is the official Elrond wallet app for the [Ledger Nano S](https://www.ledgerwallet.com/products/ledger-nano-s).

## Installation

Before proceeding with the installation, please make sure your device is up-to-date with the latest firmware.

Download the latest `*.hex` file from our [releases page](https://github.com/ElrondNetwork/ledger-elrond/releases):

```
wget https://github.com/ElrondNetwork/ledger-elrond/releases/download/v1.0.0/elrond-ledger-app-v1.0.0.hex
```

Install the Python package `ledgerblue`:

```
pip3 install --user --upgrade --no-cache-dir ledgerblue
```

Configure your OS to [enable the connectivity](https://support.ledger.com/hc/en-us/articles/115005165269) with the device. For Linux, this is as follows:

```
wget -q -O - https://raw.githubusercontent.com/LedgerHQ/udev-rules/master/add_udev_rules.sh | sudo bash
```

Load the app on the device:

```
python3 -m ledgerblue.loadApp --curve ed25519 --path "44'/508'" --appFlags 0x240 --tlv --targetId 0x31100004 --targetVersion=1.6.0 --delete --appName Elrond --appVersion 1.0.0 --fileName elrond-ledger-app-v1.0.0.hex --dataSize 64 --icon "010000000000ffffff00000000cc19ec1b1004280a4c198c184c19280a1004ec1bcc19000000000000"
```

To remove the app from the device, issue the following command:

```
python3 -m ledgerblue.deleteApp --targetId 0x31100004 --appName Elrond
```

## Testing

The `testApp` directory contains a sample Go program for interacting with the app on your Ledger and sending/signing transactions.

## Development environment: building and installing

To build and install the app on your Nano S, you must first set up the environment:

```$ source prepare-devenv s```

```$ make load```

To remove the app from the device, run:

```$ make delete```

