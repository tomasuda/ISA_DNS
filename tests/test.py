import pytest
import socket
import random
import struct
import time

def create_dns_query(domain, query_type=1, query_class=1):
    """Create a DNS query for a specific domain"""
    # Convert domain to DNS format (length-prefixed labels)
    domain_parts = domain.encode('ascii').split(b'.')
    qname = b''
    for part in domain_parts:
        qname += bytes([len(part)]) + part
    qname += b'\x00'  # Null terminator
    
    # Build DNS query
    transaction_id = random.randint(0, 65535).to_bytes(2, 'big')
    
    query = bytearray()
    query.extend(transaction_id)          # Transaction ID
    query.extend(b"\x01\x00")             # Flags: Standard query
    query.extend(b"\x00\x01")             # Questions: 1
    query.extend(b"\x00\x00")             # Answers: 0
    query.extend(b"\x00\x00")             # Authority: 0
    query.extend(b"\x00\x00")             # Additional: 0
    query.extend(qname)                   # QNAME
    query.extend(query_type.to_bytes(2, 'big'))   # QTYPE
    query.extend(query_class.to_bytes(2, 'big'))  # QCLASS
    
    return transaction_id, bytes(query)

def parse_dns_response(response):
    """Parse DNS response and return basic information"""
    if len(response) < 12:
        return None
    
    header = {
        'transaction_id': response[0:2],
        'flags': response[2:4],
        'questions': int.from_bytes(response[4:6], 'big'),
        'answers': int.from_bytes(response[6:8], 'big'),
        'authority': int.from_bytes(response[8:10], 'big'),
        'additional': int.from_bytes(response[10:12], 'big')
    }
    
    # Check if it's a response (QR bit)
    header['is_response'] = (response[2] & 0x80) == 0x80
    
    # Get response code
    header['rcode'] = response[3] & 0x0F
    
    return header

class TestLiveDNSServer:
    """Test your live DNS server running on port 5353"""
    
    @pytest.fixture
    def dns_socket(self):
        """Create and return a UDP socket for DNS queries"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(10.0)  # 10 second timeout
        yield sock
        sock.close()
    
    def test_server_is_running(self, dns_socket):
        """Test that the DNS server is running and responsive"""
        # Send a simple DNS query
        transaction_id, query = create_dns_query("google.com")
        
        dns_socket.sendto(query, ('127.0.0.1', 5353))
        
        try:
            response, addr = dns_socket.recvfrom(1024)
            assert len(response) > 0, "Empty response from server"
            print(f"Received response of {len(response)} bytes from {addr}")
            
        except socket.timeout:
            pytest.fail("DNS server did not respond within timeout - is it running on port 5353?")
    
    def test_valid_dns_response(self, dns_socket):
        """Test that the server returns valid DNS responses"""
        transaction_id, query = create_dns_query("example.com")
        
        dns_socket.sendto(query, ('127.0.0.1', 5353))
        response, addr = dns_socket.recvfrom(1024)
        
        # Parse response
        header = parse_dns_response(response)
        assert header is not None, "Invalid DNS response"
        
        # Validate response
        assert header['transaction_id'] == transaction_id, "Transaction ID mismatch"
        assert header['is_response'] == True, "Not a DNS response (QR bit not set)"
        assert header['rcode'] == 0, f"DNS error response (RCODE: {header['rcode']})"
        assert header['answers'] >= 0, "Invalid answer count"
        
        print(f"Valid DNS response: {header['answers']} answers, RCODE: {header['rcode']}")
    
    def test_multiple_domains(self, dns_socket):
        """Test DNS resolution for multiple domains"""
        test_domains = [
            "google.com",
            "github.com", 
            "stackoverflow.com",
            "wikipedia.org"
        ]
        
        for domain in test_domains:
            transaction_id, query = create_dns_query(domain)
            
            dns_socket.sendto(query, ('127.0.0.1', 5353))
            response, addr = dns_socket.recvfrom(1024)
            
            header = parse_dns_response(response)
            assert header is not None, f"Invalid response for {domain}"
            assert header['transaction_id'] == transaction_id, f"Transaction ID mismatch for {domain}"
            assert header['is_response'] == True, f"Not a response for {domain}"
            
            print(f"✓ {domain}: {header['answers']} answers")
    
    def test_nonexistent_domain(self, dns_socket):
        """Test response for non-existent domain"""
        # Use a likely non-existent domain
        transaction_id, query = create_dns_query("this-domain-probably-does-not-exist-12345.com")
        
        dns_socket.sendto(query, ('127.0.0.1', 5353))
        response, addr = dns_socket.recvfrom(1024)
        
        header = parse_dns_response(response)
        assert header is not None, "Invalid response for non-existent domain"
        
        # Non-existent domains typically return RCODE 3 (Name Error)
        # But some DNS servers may return 0 with no answers
        print(f"Non-existent domain response: RCODE {header['rcode']}")
    
    def test_domain_blocking(self, dns_socket):
        """Test if domain blocking is working"""
        # Test with domains that might be in your blocklist
        blocked_domains = [
            "x.x.x.x.x.x.x.x.x.x.x.xtracker.logimeter.com",
            ".r.r.r.r.r.r.r.r.r.r.r.xxxcounter.com",
            "r.r.r.r.r.r.r.r.r.r.rxxxmyself.com",
            "k.k.k.k.k.k.k.k.k.k.ky.ibsys.com",
            ".k.k.k.g.g.g.yab-adimages.s3.amazonaws.com"
        ]
        
        for domain in blocked_domains:
            transaction_id, query = create_dns_query(domain)
            
            dns_socket.sendto(query, ('127.0.0.1', 5353))
            response, addr = dns_socket.recvfrom(1024)
            
            header = parse_dns_response(response)
            assert header is not None, f"Invalid response for {domain}"
            
            # Check if domain is blocked (your server might return RCODE 5 for refused)
            if header['rcode'] == 5:  # REFUSED
                print(f"✓ {domain} is blocked (REFUSED)")
            elif header['answers'] == 0 and header['rcode'] == 0:
                print(f"○ {domain}: No answers (might be blocked or non-existent)")
            else:
                print(f"  {domain}: Allowed ({header['answers']} answers)")
    
    def test_concurrent_queries(self, dns_socket):
        """Test handling of multiple concurrent queries"""
        domains = ["google.com", "example.com", "test.com"]
        queries = []
        
        # Send all queries first
        for domain in domains:
            transaction_id, query = create_dns_query(domain)
            dns_socket.sendto(query, ('127.0.0.1', 5353))
            queries.append(transaction_id)
        
        # Then receive all responses
        responses = []
        for i in range(len(domains)):
            try:
                response, addr = dns_socket.recvfrom(1024)
                responses.append(response)
            except socket.timeout:
                pytest.fail(f"Timeout waiting for response {i+1}")
        
        assert len(responses) == len(domains), "Missing responses"
        
        for i, response in enumerate(responses):
            header = parse_dns_response(response)
            assert header is not None, f"Invalid response {i+1}"
            assert header['is_response'] == True, f"Response {i+1} not a DNS response"
            print(f"Concurrent query {i+1}: OK")

def test_manual_server_check():
    """Manual test to check if server is running before automated tests"""
    print("\n" + "="*50)
    print("Checking if DNS server is running on port 5353...")
    
    # Simple socket connection test
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(2.0)
        
        # Send a minimal DNS query
        test_query = b"\x00\x00\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        sock.sendto(test_query, ('127.0.0.1', 5353))
        
        response, addr = sock.recvfrom(1024)
        print("✓ DNS server is running and responsive")
        sock.close()
        return True
        
    except socket.timeout:
        print("✗ DNS server is not responding on port 5353")
        print("Please start your server first:")
        print("  ./build/dns_forwarder -p 5353 -s 8.8.8.8 -v")
        return False
    except Exception as e:
        print(f"✗ Error connecting to DNS server: {e}")
        return False

# Run the manual check before all tests
def pytest_sessionstart(session):
    """Check if server is running before starting tests"""
    if not test_manual_server_check():
        print("\nCannot run tests - DNS server not available")
        # You might want to exit or skip tests here