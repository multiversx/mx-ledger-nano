# Elrond app for the Ledger Nano S

## Introduction

This is the official Elrond wallet app for the [Ledger Nano S](https://www.ledgerwallet.com/products/ledger-nano-s).

## Building and installing

To build and install the app on your Nano S, you must first set up the environment:

```$ source prepare-devenv s```

```$ make load```

To remove the app from the device, run:

```$ make delete```

## Testing

The `testApp` directory contains a sample Go program for interacting with the app on your Ledger and sending/signing transactions.
