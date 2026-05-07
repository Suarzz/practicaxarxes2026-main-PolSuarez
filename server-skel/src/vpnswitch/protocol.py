from enum import IntEnum


class OpCode(IntEnum):
    REGISTER = 1
    AUTH = 2
    TRAFFIC = 3
    KEEPALIVE = 4
    ACK = 0x05
    REJECT = 0x06