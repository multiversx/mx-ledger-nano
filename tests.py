#!/usr/bin/env python3

"""
Functional tests for the Elrond app. Compatible with a real Ledger Nano S device
and Speculos. Speculos usage:

  $ ./speculos.py ~/app-elrond/bin/app.elf &
  $ LEDGER_PROXY_ADDRESS=127.0.0.1 LEDGER_PROXY_PORT=9999 pytest-3 tests.py -s
"""

import base64
import binascii
import json
import pytest
import re
import sys
import time
from enum import IntEnum

from ledgerwallet.client import LedgerClient, CommException
from ledgerwallet.transport import enumerate_devices

CLA = 0xED

class Ins(IntEnum):
    GET_APP_VERSION       = 0x01
    GET_APP_CONFIGURATION = 0x02
    GET_ADDR              = 0x03
    SIGN_TX               = 0x04
    SET_ADDR              = 0x05
    SIGN_MSG              = 0x06
    SIGN_TX_HASH          = 0x07

class P1(IntEnum):
    CONFIRM     = 0x01
    NON_CONFIRM = 0x00
    FIRST       = 0x00
    MORE        = 0x80

class P2(IntEnum):
    DISPLAY_BECH32 = 0x00
    DISPLAY_HEX    = 0x01

class Error(IntEnum):
    USER_DENIED            = 0x6985
    UNKNOWN_INSTRUCTION    = 0x6D00
    WRONG_CLA              = 0x6E00
    SIGNATURE_FAILED       = 0x6E10
    SIGN_TX_DEPRECATED     = 0x6E11
    INVALID_ARGUMENTS      = 0x6E01
    INVALID_MESSAGE        = 0x6E02
    INVALID_P1             = 0x6E03
    MESSAGE_TOO_LONG       = 0x6E04
    RECEIVER_TOO_LONG      = 0x6E05
    AMOUNT_TOO_LONG        = 0x6E06
    CONTRACT_DATA_DISABLED = 0x6E07
    MESSAGE_INCOMPLETE     = 0x6E08
    WRONG_TX_VERSION       = 0x6E09
    NONCE_TOO_LONG         = 0x6E0A
    INVALID_AMOUNT         = 0x6E0B
    INVALID_FEE            = 0x6E0C
    PRETTY_FAILED          = 0x6E0D

@pytest.fixture(scope="module")
def client():
    devices = enumerate_devices()
    if len(devices) == 0:
        print("No Ledger device has been found.")
        sys.exit(0)

    return LedgerClient(devices[0], cla=CLA)

class TestGetAppVersion:
    INS = Ins.GET_APP_VERSION

    def test_get_app_version(self, client):
        data = client.apdu_exchange(self.INS, b"", P1.FIRST, 0)
        assert re.match("\d+\.\d+\.\d+", data.decode("ascii"))

class TestGetAppConfiguration:
    INS = Ins.GET_APP_CONFIGURATION

    def test_get_app_configuration(self, client):
        data = client.apdu_exchange(self.INS, b"", P1.FIRST, 0)
        assert len(data) == 6

class TestGetAddr:
    INS = Ins.GET_ADDR

    def test_get_addr(self, client):
        account = 1
        index = 1
        payload = account.to_bytes(4, "big") + index.to_bytes(4, "big")
        data = client.apdu_exchange(self.INS, payload, P1.NON_CONFIRM, P2.DISPLAY_HEX)
        assert re.match("^@[0-9a-f]{64}$", data.decode("ascii"))

    def test_get_addr_too_long(self, client):
        payload = int(0).to_bytes(4, "big") * 3
        with pytest.raises(CommException) as e:
            client.apdu_exchange(self.INS, payload, P1.NON_CONFIRM, P2.DISPLAY_HEX)
        assert e.value.sw == Error.INVALID_ARGUMENTS

class TestSignTx:
    INS = Ins.SIGN_TX

    def test_sign_tx_deprecated(self, client):
        with pytest.raises(CommException) as e:
            client.apdu_exchange(self.INS, b"", 0, 0)
        assert e.value.sw == Error.SIGN_TX_DEPRECATED


class TestSignMsg:
    INS = Ins.SIGN_MSG

    def _exchange_tx(self, client, payload):
        max_size = 251
        p1 = P1.FIRST
        payload = len(payload).to_bytes(4, "big") + payload
        while payload:
            client.apdu_exchange(self.INS, payload[:max_size], p1, 0)
            payload = payload[max_size:]
            if p1 == p1.FIRST:
                p1 = P1.MORE

    def test_sign_msg_invalid_len(self, client):
        client.apdu_exchange(self.INS, b"\x00\xff\xff\xff", P1.FIRST, 0)

    def test_sign_msg(self, client):
        payload = b"abcd"
        self._exchange_tx(client, payload)

    def test_sign_msg_long(self, client):
        payload = b"a" * 512
        self._exchange_tx(client, payload)

    def test_sign_msg_too_long(self, client):
        payload = b"abcd"
        payload = int(3).to_bytes(4, "big") + payload
        with pytest.raises(CommException) as e:
            client.apdu_exchange(self.INS, payload, P1.FIRST, 0)
        assert e.value.sw == Error.MESSAGE_TOO_LONG

class TestSignTxHash:
    INS = Ins.SIGN_TX_HASH

    def _exchange_tx(self, client, payload):
        max_size = 251
        p1 = P1.FIRST
        while payload:
            client.apdu_exchange(self.INS, payload[:max_size], p1, 0)
            payload = payload[max_size:]
            if p1 == p1.FIRST:
                p1 = P1.MORE

    def test_sign_tx_valid(self, client):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        self._exchange_tx(client, payload)

    def test_sign_tx_valid_large(self, client):
        payload  = b'{"nonce":1234,"value":"'
        payload += b'1' * 31
        payload += b'","receiver":"'
        payload += b'r' * 63
        payload += b'","sender":"'
        payload += b's' * 63
        payload += b'","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        self._exchange_tx(client, payload)

    def test_sign_tx_valid_large_nonce(self, client):
        # nonce is a 64-bit unsigned integer
        payload = b'{"nonce":18446744073709551615,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        self._exchange_tx(client, payload)

    def test_sign_tx_valid_large_amount(self, client):
        payload = b'{"nonce":1234,"value":"1234567890123456789012345678901","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        self._exchange_tx(client, payload)

    def test_sign_tx_invalid_nonce(self, client):
        payload = b'{"nonce":{},"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with pytest.raises(CommException) as e:
            self._exchange_tx(client, payload)
        assert e.value.sw == Error.INVALID_MESSAGE

    def test_sign_tx_invalid_amount(self, client):
        payload = b'{"nonce":1234,"value":"A5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with pytest.raises(CommException) as e:
            self._exchange_tx(client, payload)
        assert e.value.sw == Error.INVALID_AMOUNT

    def test_sign_tx_invalid_fee(self, client):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":2000000000000000000000,"gasLimit":20000000000000000,"chainID":"1","version":2}'
        with pytest.raises(CommException) as e:
            self._exchange_tx(client, payload)
        assert e.value.sw == Error.INVALID_FEE
