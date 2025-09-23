import pytest
import socket
import random

@pytest.fixture
def sample_dns_query():
    """Fixture providing a sample DNS query"""
    return bytes.fromhex(
        "aa550100000100000000000006676f6f676c6503636f6d0000010001"
    )

@pytest.fixture
def dns_client_socket():
    """Fixture providing a UDP socket for DNS queries"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    yield sock
    sock.close()

@pytest.fixture
def random_transaction_id():
    """Fixture providing random transaction IDs"""
    return random.randint(0, 65535)