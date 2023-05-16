#!/usr/bin/env python3

"""
Functional tests for the MultiversX app (previously Elrond). Compatible with a real Ledger device and Speculos

Simple usage:
pytest ./tests/

Example usage of only the test test_sign_tx_valid_simple on Speculos on LNX
pytest ./tests/ -v --nanox --display -k test_sign_tx_valid_simple

Please refer to the usage file for all available options
"""

from contextlib import contextmanager
from typing import List, Generator, Dict
from requests import exceptions

import base64
import binascii
import json
import pytest
import re
import sys
from enum import IntEnum
from pathlib import Path

from ragger.navigator import NavInsID, NavIns
from ragger.backend.interface import RAPDU, RaisePolicy

CLA = 0xED

LEDGER_MAJOR_VERSION = 1
LEDGER_MINOR_VERSION = 0
LEDGER_PATCH_VERSION = 21

class Ins(IntEnum):
    GET_APP_VERSION       = 0x01
    GET_APP_CONFIGURATION = 0x02
    GET_ADDR              = 0x03
    SIGN_TX               = 0x04
    SET_ADDR              = 0x05
    SIGN_MSG              = 0x06
    SIGN_TX_HASH          = 0x07
    PROVIDE_ESDT_INFO     = 0x08 # TODO add test for this APDU
    SIGN_MSG_AUTH_TOKEN   = 0x09

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
def send_async_sign_message(backend, ins, payload: bytes) -> Generator[None, None, None]:
    payload_splited = [payload[x:x + MAX_SIZE] for x in range(0, len(payload), MAX_SIZE)]
    p1 = P1.FIRST
    if len(payload_splited) > 1:
        for p in payload_splited[:-1]:
            backend.exchange(CLA, ins, p1, 0, p)
            p1 = P1.MORE

    with backend.exchange_async(CLA,
                                ins,
                                p1,
                                0,
                                payload_splited[-1]):
        yield


class TestMenu:

    def test_menu(self, backend, navigator, test_name):
        if backend.firmware.device.startswith("nano"):
            nav_ins = [NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK]
        elif backend.firmware.device == "stax":
            nav_ins = [NavInsID.USE_CASE_HOME_SETTINGS,
                       NavInsID.USE_CASE_SETTINGS_NEXT,
                       NavInsID.USE_CASE_SETTINGS_PREVIOUS,
                       NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT,
                       NavInsID.USE_CASE_HOME_QUIT]

        with pytest.raises(exceptions.ConnectionError):
            navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins, screen_change_before_first_instruction=False)


class TestGetAppVersion:

    def test_get_app_version(self, backend):
        data = backend.exchange(CLA, Ins.GET_APP_VERSION, P1.FIRST, 0, b"").data
        version = data.decode("ascii").split('.')
        assert version[0] == str(LEDGER_MAJOR_VERSION)
        assert version[1] == str(LEDGER_MINOR_VERSION)
        assert version[2] == str(LEDGER_PATCH_VERSION)


class TestGetAppConfiguration:

    def test_get_app_configuration(self, backend):
        data = backend.exchange(CLA, Ins.GET_APP_CONFIGURATION, P1.FIRST, 0, b"").data
        assert len(data) == 14
        assert data[0] == 0 or data[0] == 1                 # N_storage.setting_contract_data
        # data[1] is not to be taken into account anymore
        # data[2] is not to be taken into account anymore
        assert data[3] == LEDGER_MAJOR_VERSION              # LEDGER_MAJOR_VERSION
        assert data[4] == LEDGER_MINOR_VERSION              # LEDGER_MINOR_VERSION
        assert data[5] == LEDGER_PATCH_VERSION              # LEDGER_PATCH_VERSION
        # data[6:10] is the bip32_account
        # data[10:14] is the bip32_address_index

    def test_toggle_contract_data(self, backend, navigator, test_name):
        # init enabled
        assert backend.exchange(CLA, Ins.GET_APP_CONFIGURATION, P1.FIRST, 0, b"").data[0] == 1

        # switch to disabled
        if backend.firmware.device.startswith("nano"):
            nav_ins = [NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.LEFT_CLICK,
                       NavInsID.BOTH_CLICK]
        elif backend.firmware.device == "stax":
            nav_ins = [NavInsID.USE_CASE_HOME_SETTINGS,
                       NavIns(NavInsID.TOUCH, (350,115)),
                       NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]

        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name + "_0", nav_ins, screen_change_before_first_instruction=False)
        assert backend.exchange(CLA, Ins.GET_APP_CONFIGURATION, P1.FIRST, 0, b"").data[0] == 0

        # switch back to enabled
        if backend.firmware.device.startswith("nano"):
            nav_ins = [NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.BOTH_CLICK,
                       NavInsID.RIGHT_CLICK,
                       NavInsID.BOTH_CLICK]
        elif backend.firmware.device == "stax":
            nav_ins = [NavInsID.USE_CASE_HOME_SETTINGS,
                       NavIns(NavInsID.TOUCH, (350,115)),
                       NavInsID.USE_CASE_SETTINGS_MULTI_PAGE_EXIT]

        navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name + "_1", nav_ins, screen_change_before_first_instruction=False)
        assert backend.exchange(CLA, Ins.GET_APP_CONFIGURATION, P1.FIRST, 0, b"").data[0] == 1


class TestGetAddr:

    def test_get_addr_non_confirm(self, backend):
        account = 1
        index = 1
        payload = account.to_bytes(4, "big") + index.to_bytes(4, "big")
        data = backend.exchange(CLA, Ins.GET_ADDR, P1.NON_CONFIRM, P2.DISPLAY_HEX, payload).data
        assert re.match("^@[0-9a-f]{64}$", data.decode("ascii"))

    def test_get_addr_confirm_ok(self, backend, navigator, test_name):
        account = 1
        index = 1
        payload = account.to_bytes(4, "big") + index.to_bytes(4, "big")
        with backend.exchange_async(CLA, Ins.GET_ADDR, P1.CONFIRM, P2.DISPLAY_HEX, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Approve",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                nav_ins = [
                           NavInsID.USE_CASE_REVIEW_TAP,
                           NavIns(NavInsID.TOUCH, (200,346)),
                           NavInsID.USE_CASE_ADDRESS_CONFIRMATION_EXIT_QR,
                           NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CONFIRM,
                           NavInsID.USE_CASE_STATUS_DISMISS]
                navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)
        assert re.match("^@[0-9a-f]{64}$", backend.last_async_response.data.decode("ascii"))

    def test_get_addr_confirm_refused(self, backend, navigator, test_name):
        account = 1
        index = 1
        payload = account.to_bytes(4, "big") + index.to_bytes(4, "big")
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with backend.exchange_async(CLA, Ins.GET_ADDR, P1.CONFIRM, P2.DISPLAY_HEX, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Reject",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                nav_ins = [
                           NavInsID.USE_CASE_REVIEW_TAP,
                           NavInsID.USE_CASE_ADDRESS_CONFIRMATION_CANCEL,
                           NavInsID.USE_CASE_STATUS_DISMISS]
                navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)
        assert backend.last_async_response.status == Error.USER_DENIED

    def test_get_addr_too_long(self, backend):
        payload = int(0).to_bytes(4, "big") * 3
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = backend.exchange(CLA, Ins.GET_ADDR, P1.NON_CONFIRM, P2.DISPLAY_HEX, payload)
        assert rapdu.status == Error.INVALID_ARGUMENTS


class TestSignTx:

    def test_sign_tx_deprecated(self, backend):
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = backend.exchange(CLA, Ins.SIGN_TX, 0, 0, b"")
        assert rapdu.status == Error.SIGN_TX_DEPRECATED


class TestSignMsg:

    def test_sign_msg_invalid_len(self, backend):
        backend.exchange(CLA, Ins.SIGN_MSG, P1.FIRST, 0, b"\x00\xff\xff\xff")

    def test_sign_msg_short_ok(self, backend, navigator, test_name):
        payload = b"abcd"
        payload = len(payload).to_bytes(4, "big") + payload
        with send_async_sign_message(backend, Ins.SIGN_MSG, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign message",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                # Navigate a bit through rejection screens before confirming
                nav_ins = [NavInsID.USE_CASE_REVIEW_REJECT,
                           NavInsID.USE_CASE_CHOICE_REJECT,
                           NavInsID.USE_CASE_REVIEW_TAP,
                           NavInsID.USE_CASE_REVIEW_TAP,
                           NavInsID.USE_CASE_REVIEW_PREVIOUS,
                           NavInsID.USE_CASE_REVIEW_REJECT,
                           NavInsID.USE_CASE_CHOICE_REJECT,
                           NavInsID.USE_CASE_REVIEW_TAP,
                           NavInsID.USE_CASE_REVIEW_CONFIRM]
                navigator.navigate_and_compare(ROOT_SCREENSHOT_PATH, test_name, nav_ins)

    def test_sign_msg_short_rejected(self, backend, navigator, test_name):
        payload = b"abcd"
        payload = len(payload).to_bytes(4, "big") + payload
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(backend, Ins.SIGN_MSG, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Reject",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_REJECT,
                                                           NavInsID.USE_CASE_CHOICE_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
        assert backend.last_async_response.status == Error.USER_DENIED

    def test_sign_msg_long(self, backend, navigator, test_name):
        payload = b"a" * 512
        payload = len(payload).to_bytes(4, "big") + payload
        with send_async_sign_message(backend, Ins.SIGN_MSG, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign message",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_msg_too_long(self, backend):
        payload = b"abcd"
        payload = (len(payload) - 1).to_bytes(4, "big") + payload
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = backend.exchange(CLA, Ins.SIGN_MSG, P1.FIRST, 0, payload)
        assert rapdu.status == Error.MESSAGE_TOO_LONG


class TestSignTxHash:
    # TODO: add test for esdt flow

    def test_sign_tx_valid_simple_no_data_confirmed(self, backend, navigator, test_name):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign transaction",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_tx_valid_simple_no_data_rejected(self, backend, navigator, test_name):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Reject",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_REJECT,
                                                           NavInsID.USE_CASE_CHOICE_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
        assert backend.last_async_response.status == Error.USER_DENIED

    def test_sign_tx_valid_simple_data_confirmed(self, backend, navigator, test_name):
        # TODO: use actual data value that makes sense
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2,"data":"test"}'
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign transaction",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_tx_valid_large_receiver(self, backend, navigator, test_name):
        payload  = b'{"nonce":1234,"value":"'
        payload += b'1' * 31
        payload += b'","receiver":"'
        payload += b'r' * 63
        payload += b'","sender":"'
        payload += b's' * 63
        payload += b'","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign transaction",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_tx_valid_large_nonce(self, backend, navigator, test_name):
        # nonce is a 64-bit unsigned integer
        payload = b'{"nonce":18446744073709551615,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign transaction",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_tx_valid_large_amount(self, backend, navigator, test_name):
        payload = b'{"nonce":1234,"value":"1234567890123456789012345678901","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Sign transaction",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_tx_invalid_nonce(self, backend):
        payload = b'{"nonce":{},"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            # error return expected
            pass
        assert backend.last_async_response.status == Error.INVALID_MESSAGE

    def test_sign_tx_invalid_amount(self, backend):
        payload = b'{"nonce":1234,"value":"A5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            # error return expected
            pass
        assert backend.last_async_response.status == Error.INVALID_AMOUNT

    def test_sign_tx_invalid_fee(self, backend):
        payload = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":2000000000000000000000,"gasLimit":20000000000000000,"chainID":"1","version":2}'
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(backend, Ins.SIGN_TX_HASH, payload):
            # error return expected
            pass
        assert backend.last_async_response.status == Error.INVALID_FEE


class TestSignMsgAuthToken:

    def test_sign_msg_auth_token_ok(self, backend, navigator, test_name):
        payload:bytes = b""
        payload += (0).to_bytes(4, "big") # account index
        payload += (0).to_bytes(4, "big") # address index
        token = b"aHEOcHM6Ly93YWxsZXQubXVsdGI2ZXJzeC5jb20.726757b8ca0b552199af4f0697eacd95940916044f21824f9ef8767e654b95cb.86400.eyJ0aW1lc3RhbXAiOjE2ODM3OTQzMjJ900aHEOcaHEOcHM6Ly93YWxsZXQubXVsdGI2ZXJzeC5jb20.726757b8ca0b552199af4f0697eacd95940916044f21824f9ef8767e654b95AZEQ"
        payload += (len(token)).to_bytes(4, "big")
        payload += token
        with send_async_sign_message(backend, Ins.SIGN_MSG_AUTH_TOKEN, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Authorize",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)

    def test_sign_msg_auth_token_refused(self, backend, navigator, test_name):
        payload:bytes = b""
        payload += (0).to_bytes(4, "big") # account index
        payload += (0).to_bytes(4, "big") # address index
        token = b"BLOB"
        payload += (len(token)).to_bytes(4, "big")
        payload += token
        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        with send_async_sign_message(backend, Ins.SIGN_MSG_AUTH_TOKEN, payload):
            if backend.firmware.device.startswith("nano"):
                navigator.navigate_until_text_and_compare(NavInsID.RIGHT_CLICK,
                                                          [NavInsID.BOTH_CLICK],
                                                          "Reject",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
            elif backend.firmware.device == "stax":
                navigator.navigate_until_text_and_compare(NavInsID.USE_CASE_REVIEW_TAP,
                                                          [NavInsID.USE_CASE_REVIEW_REJECT,
                                                           NavInsID.USE_CASE_CHOICE_CONFIRM,
                                                           NavInsID.USE_CASE_STATUS_DISMISS],
                                                          "Hold to sign",
                                                          ROOT_SCREENSHOT_PATH,
                                                          test_name)
        assert backend.last_async_response.status == Error.USER_DENIED


class TestState:

    def test_invalid_state(self, backend):
        """Ensures there is no state confusion between tx and message signatures"""

        tx = b'{"nonce":1234,"value":"5678","receiver":"efgh","sender":"abcd","gasPrice":50000,"gasLimit":20,"chainID":"1","version":2}'

        payload = int(1).to_bytes(4, "big")
        backend.exchange(CLA, Ins.SIGN_MSG, P1.FIRST, 0, payload)

        backend.exchange(CLA, Ins.SIGN_TX_HASH, P1.FIRST, 0, tx[:-1])

        backend.raise_policy = RaisePolicy.RAISE_NOTHING
        rapdu = backend.exchange(CLA, Ins.SIGN_MSG, P1.MORE, 0, tx[-1:])
        assert rapdu.status == Error.INVALID_MESSAGE
