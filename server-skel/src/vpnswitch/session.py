from dataclasses import dataclass, field
from enum import Enum
import time



class SessionState(str, Enum):
    none = "none"
    registering = "registering"
    authenticated = "authenticated"

@dataclass
class VpnSession:
    ip: str
    port: int
    state: SessionState = SessionState.none
    last_seen: float = field(default_factory=time.time)
    pktsIn: int = 0
    pktsOut: int = 0
    bytesIn: int = 0
    bytesOut: int = 0