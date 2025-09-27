from enum import Enum

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:143.0) Gecko/20100101 Firefox/143.0",
}


class Competition(Enum):
    UCL = "ucl"
    UEL = "uel"
    UECL = "uecl"
