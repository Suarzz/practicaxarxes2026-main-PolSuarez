def is_valid_auth_payload(payload: bytes) -> bool:
    return len(payload) == 8 and payload.isalnum()
