from typing import Tuple
from pathlib import Path
import os

makefile_relative_path = "../Makefile"

makefile_path = (Path(os.path.dirname(os.path.realpath(__file__))) / Path(makefile_relative_path)).resolve()

def get_version_from_makefile() -> Tuple[int, int, int]:
    '''
    Parse the app Makefile to automatically
    '''
    APPVERSION_M = -1
    APPVERSION_N = -1
    APPVERSION_P = -1
    with open(makefile_path) as myfile:
        for line in myfile:
            if line.startswith("APPVERSION_M"):
                _, var = line.partition("=")[::2]
                APPVERSION_M = int(var.strip())
            if line.startswith("APPVERSION_N"):
                _, var = line.partition("=")[::2]
                APPVERSION_N = int(var.strip())
            if line.startswith("APPVERSION_P"):
                _, var = line.partition("=")[::2]
                APPVERSION_P = int(var.strip())

    return APPVERSION_M, APPVERSION_N, APPVERSION_P
