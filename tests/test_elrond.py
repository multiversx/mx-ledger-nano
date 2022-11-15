#!/usr/bin/env python3

"""
Functional tests for the Elrond app. Compatible with a real Ledger device and Speculos

Simple usage:
pytest ./tests/

Example usage of only the test test_sign_tx_valid_simple on Speculos on LNX
pytest ./tests/ -v --nanox --display -k test_sign_tx_valid_simple

Please refer to the conftest file for all available options
"""

from contextlib import contextmanager
from typing import List, Generator, Dict

import base64
import binascii
import json
import pytest
import re
import sys
import time
from enum import IntEnum
from pathlib import Path

from ragger.navigator import NavInsID, NavIns, NanoNavigator
from ragger.backend.interface import RAPDU, RaisePolicy
from .utils import create_simple_nav_instructions

CLA = 0xED

LEDGER_MAJOR_VERSION = 1
LEDGER_MINOR_VERSION = 0
LEDGER_PATCH_VERSION = 19

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

MAX_SIZE = 251
ROOT_SCREENSHOT_PATH = Path(__file__).parent.resolve()


@contextmanager
def send_async_sign_message(client, ins, payload: bytes) -> Generator[None, None, None]:
    payload_splited = [payload[x:x + MAX_SIZE] for x in range(0, len(payload), MAX_SIZE)]
    p1 = P1.FIRST
    if len(payload_splited) > 1:
        for p in payload_splited[:-1]:
            client.exchange(CLA, ins, p1, 0, p)
            p1 = P1.MORE

    with client.exchange_async(CLA,
                               ins,
                               p1,
                               0,
                               payload_splited[-1]):
        yield


class TestGetAppVersion:

    def test_get_app_version(self, client, backend):
        data = client.exchange(CLA, Ins.GET_APP_VERSION, P1.FIRST, 0, b"").data
        version = data.decode("ascii").split('.')
        assert version[0] == str(LEDGER_MAJOR_VERSION)
        assert version[1] == str(LEDGER_MINOR_VERSION)
        assert version[2] == str(LEDGER_PATCH_VERSION)


class TestGetAppConfiguration:

    def test_get_app_configuration(self, client, backend):
        data = client.exchange(CLA, Ins.GET_APP_CONFIGURATION, P1.FIRST, 0, b"").data
        assert len(data) == 14
        assert data[0] == 0 or data[0] == 1                         # N_storage.setting_contract_data
        # print(data[1])                                            # not to be taken into account anymore
        # print(data[2])                                            # not to be taken into account anymore
        assert data[3] == LEDGER_MAJOR_VERSION                      # LEDGER_MAJOR_VERSION
        assert data[4] == LEDGER_MINOR_VERSION                      # LEDGER_MINOR_VERSION
        assert data[5] == LEDGER_PATCH_VERSION                      # LEDGER_PATCH_VERSION
        # assert int.from_bytes(data[6:10], byteorder="big") == 0     # bip32_account
        # assert int.from_bytes(data[10:14], byteorder="big") == 0    # bip32_address_index


class TestGetAddr:

    def test_get_addr(self, client, backend):
        account = 1
        index = 1
        payload = account.to_bytes(4, "big") + index.to_bytes(4, "big")
        data = client.exchange(CLA, Ins.GET_ADDR, P1.NON_CONFIRM, P2.DISPLAY_HEX, payload).data
        assert re.match("^@[0-9a-f]{64}$", data.decode("ascii"))

    def test_get_addr_too_long(self, client, backend):
        payload = int(0).to_bytes(4, "big") * 3
        client.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = client.exchange(CLA, Ins.GET_ADDR, P1.NON_CONFIRM, P2.DISPLAY_HEX, payload)
        assert rapdu.status == Error.INVALID_ARGUMENTS


class TestSignTx:

    def test_sign_tx_deprecated(self, client, backend):
        client.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = client.exchange(CLA, Ins.SIGN_TX, 0, 0, b"")
        assert rapdu.status == Error.SIGN_TX_DEPRECATED


class TestSignMsg:

    def test_sign_msg_invalid_len(self, client, backend):
        client.exchange(CLA, Ins.SIGN_MSG, P1.FIRST, 0, b"\x00\xff\xff\xff")

    def test_sign_msg_short(self, client, backend, navigator, test_name):
        payload = b"abcd"
        payload = len(payload).to_bytes(4, "big") + payload
        with send_async_sign_message(client, Ins.SIGN_MSG, payload):
            if client.firmware.device == "nanos":
                nav_ins = create_simple_nav_instructions(4)
            elif client.firmware.device == "nanox" or client.firmware.device == "nanosp":
                nav_ins = create_simple_nav_instructions(2)
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.0)

    def test_sign_msg_long(self, client, backend, navigator, test_name):
        payload = b"a" * 512
        payload = len(payload).to_bytes(4, "big") + payload
        with send_async_sign_message(client, Ins.SIGN_MSG, payload):
            if client.firmware.device == "nanos":
                nav_ins = create_simple_nav_instructions(4)
            elif client.firmware.device == "nanox" or client.firmware.device == "nanosp":
                nav_ins = create_simple_nav_instructions(2)
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.0)

    def test_sign_msg_too_long(self, client, backend):
        payload = b"abcd"
        payload = (len(payload) - 1).to_bytes(4, "big") + payload
        client.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = client.exchange(CLA, Ins.SIGN_MSG, P1.FIRST, 0, payload)
        assert rapdu.status == Error.MESSAGE_TOO_LONG


class TestSignTxHash:

    def test_sign_tx_valid_simple(self, client, backend, navigator, test_name):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            if client.firmware.device == "nanos":
                nav_ins = create_simple_nav_instructions(6)
            elif client.firmware.device == "nanox" or client.firmware.device == "nanosp":
                nav_ins = create_simple_nav_instructions(4)
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.0)

    def test_sign_tx_valid_large_receiver(self, client, backend, navigator, test_name):
        payload  = b'{"nonce":1234,"value":"'
        payload += b'1' * 31
        payload += b'","receiver":"'
        payload += b'r' * 63
        payload += b'","sender":"'
        payload += b's' * 63
        payload += b'","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            if client.firmware.device == "nanos":
                nav_ins = create_simple_nav_instructions(8)
            elif client.firmware.device == "nanox" or client.firmware.device == "nanosp":
                nav_ins = create_simple_nav_instructions(4)
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.0)

    def test_sign_tx_valid_large_nonce(self, client, backend, navigator, test_name):
        # nonce is a 64-bit unsigned integer
        payload = b'{"nonce":18446744073709551615,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            if client.firmware.device == "nanos":
                nav_ins = create_simple_nav_instructions(6)
            elif client.firmware.device == "nanox" or client.firmware.device == "nanosp":
                nav_ins = create_simple_nav_instructions(4)
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.0)

    def test_sign_tx_valid_large_amount(self, client, backend, navigator, test_name):
        payload = b'{"nonce":1234,"value":"1234567890123456789012345678901","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            if client.firmware.device == "nanos":
                nav_ins = create_simple_nav_instructions(7)
            elif client.firmware.device == "nanox" or client.firmware.device == "nanosp":
                nav_ins = create_simple_nav_instructions(4)
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, first_instruction_wait=1.0)

    def test_sign_tx_invalid_nonce(self, client, backend):
        payload = b'{"nonce":{},"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        client.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            # error return expected
            pass
        assert client.last_async_response.status == Error.INVALID_MESSAGE

    def test_sign_tx_invalid_amount(self, client, backend):
        payload = b'{"nonce":1234,"value":"A5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        client.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            # error return expected
            pass
        assert client.last_async_response.status == Error.INVALID_AMOUNT

    def test_sign_tx_invalid_fee(self, client, backend):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":2000000000000000000000,"gasLimit":20000000000000000,"chainID":"1","version":2}'
        client.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(client, Ins.SIGN_TX_HASH, payload):
            # error return expected
            pass
        assert client.last_async_response.status == Error.INVALID_FEE


class TestState:

    def test_invalid_state(self, client, backend):
        """Ensures there is no state confusion between tx and message signatures"""

        tx = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'

        payload = int(1).to_bytes(4, "big")
        client.exchange(CLA, Ins.SIGN_MSG, P1.FIRST, 0, payload)

        client.exchange(CLA, Ins.SIGN_TX_HASH, P1.FIRST, 0, tx[:-1])

        client.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = client.exchange(CLA, Ins.SIGN_MSG, P1.MORE, 0, tx[-1:])
        assert rapdu.status == Error.INVALID_MESSAGE
