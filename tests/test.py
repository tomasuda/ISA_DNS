import pytest
import socket
import random
import struct
import time
import subprocess
import os
import signal
import time
from pathlib import Path


def create_dns_query(domain, query_type=1, query_class=1):
    """Create a DNS query for a specific domain"""
    domain_parts = domain.encode('ascii').split(b'.')
    qname = b''
    for part in domain_parts:
        qname += bytes([len(part)]) + part
    qname += b'\x00'  # Null terminator
    
    transaction_id = random.randint(0, 65535).to_bytes(2, 'big')
    
    query = bytearray()
    query.extend(transaction_id)
    query.extend(b"\x01\x00")
    query.extend(b"\x00\x01")
    query.extend(b"\x00\x00")
    query.extend(b"\x00\x00")
    query.extend(b"\x00\x00")
    query.extend(qname)
    query.extend(query_type.to_bytes(2, 'big'))
    query.extend(query_class.to_bytes(2, 'big'))
    
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
    
    header['is_response'] = (response[2] & 0x80) == 0x80
    header['rcode'] = response[3] & 0x0F

    print(f"Response Header: {header}")
    
    return header

class TestLiveDNSServer:
    """Test your live DNS server running on port 5353"""
    
    @pytest.fixture(scope="session")
    

    def dns_server(self, request):
        """Start the DNS server before tests and stop it after"""
        server_cmd = "./dns_test"
        data_file = "tests/test_datafile.txt"
       
        
        if not os.path.exists(server_cmd):
            pytest.fail(f"DNS server executable not found: {server_cmd}")
        
        # Build command with -f ONLY if data file exists and is provided
        cmd = [server_cmd, "-p", "5353", "-s", "8.8.8.8"]

        # Check if data file exists before adding -f parameter
        if data_file and os.path.exists(data_file):
            cmd.extend(["-f", data_file])
            print(f"Using data file: {data_file}")
        elif data_file:
            print(f"Data file not found, running without -f: {data_file}")
        
        print(f"Starting DNS server: {' '.join(cmd)}")
        
        try:
            process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
            )
            
            time.sleep(2)
            
            if process.poll() is not None:
                stdout, stderr = process.communicate()
                pytest.fail(f"DNS server failed to start:\nSTDOUT: {stdout}\nSTDERR: {stderr}")
            
            print("DNS server started successfully")
            yield process
            
        finally:
            if 'process' in locals() and process.poll() is None:
                print("Stopping DNS server...")
                process.terminate()
                try:
                    stdout, stderr = process.communicate(timeout=5)
                    if stdout:
                        print("Server output:", stdout)
                except subprocess.TimeoutExpired:
                    process.kill()
                print("DNS server stopped")
    
    
    @pytest.fixture
    def dns_socket(self):
        """Create and return a UDP socket for DNS queries"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(10.0)
        yield sock
        sock.close()
    
    def test_server_is_running(self, dns_server, dns_socket):
        """Test that the DNS server is running and responsive"""
        if dns_server is None:
            pytest.skip("Using external server")
        
        time.sleep(1)
        transaction_id, query = create_dns_query("google.com")
        dns_socket.sendto(query, ('127.0.0.1', 5353))
        
        try:
            response, addr = dns_socket.recvfrom(1024)
            assert len(response) > 0, "Empty response from server"
            print(f"Received response of {len(response)} bytes from {addr}")
        except socket.timeout:
            pytest.fail("DNS server did not respond within timeout")
    
    def test_valid_dns_response(self, dns_server, dns_socket):
        """Test that the server returns valid DNS responses"""
        transaction_id, query = create_dns_query("example.com")
        dns_socket.sendto(query, ('127.0.0.1', 5353))
        response, addr = dns_socket.recvfrom(1024)
        
        header = parse_dns_response(response)
        assert header is not None, "Invalid DNS response"
        assert header['transaction_id'] == transaction_id, "Transaction ID mismatch"
        assert header['is_response'] == True, "Not a DNS response"
        assert header['rcode'] == 0, f"DNS error response (RCODE: {header['rcode']})"
        
        print(f"Valid DNS response: {header['answers']} answers")
    
    def test_multiple_domains(self, dns_server, dns_socket):
        """Test DNS resolution for multiple domains"""
        test_domains = ["google.com", "github.com", "stackoverflow.com"]
        
        for domain in test_domains:
            transaction_id, query = create_dns_query(domain)
            dns_socket.sendto(query, ('127.0.0.1', 5353))
            response, addr = dns_socket.recvfrom(1024)
            
            header = parse_dns_response(response)
            assert header is not None, f"Invalid response for {domain}"
            assert header['transaction_id'] == transaction_id
            assert header['is_response'] == True
            print(f"✓ {domain}: {header['answers']} answers")
    
    def test_nonexistent_domain(self, dns_server, dns_socket):
        """Test response for non-existent domain"""
        transaction_id, query = create_dns_query("this-domain-probably-does-not-exist-12345.com")
        dns_socket.sendto(query, ('127.0.0.1', 5353))
        response, addr = dns_socket.recvfrom(1024)
        
        header = parse_dns_response(response)
        assert header is not None, "Invalid response"
        print(f"Non-existent domain response: RCODE {header['rcode']}")
    
    def test_domain_blocking(self, dns_server, dns_socket):
        """Test if domain blocking is working"""
        blocked_domains = [
            "example.com",  # Use simple domain for testing
            "test.com",
        ]
        
        for domain in blocked_domains:
            transaction_id, query = create_dns_query(domain)
            dns_socket.sendto(query, ('127.0.0.1', 5353))
            response, addr = dns_socket.recvfrom(1024)
            
            header = parse_dns_response(response)
            assert header is not None, f"Invalid response for {domain}"
            
            if header['rcode'] == 5:
                print(f"✓ {domain} is blocked (REFUSED)")
            elif header['answers'] == 0:
                print(f"○ {domain}: No answers")
            else:
                print(f"  {domain}: Allowed ({header['answers']} answers)")
    
    def test_concurrent_queries(self, dns_server, dns_socket):
        """Test handling of multiple concurrent queries"""
        domains = ["google.com", "example.com"]
        queries = []
        
        for domain in domains:
            transaction_id, query = create_dns_query(domain)
            dns_socket.sendto(query, ('127.0.0.1', 5353))
            queries.append(transaction_id)
        
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
            print(f"Concurrent query {i+1}: OK")
            
    def test_server_output_capture(self):
        """Capture stdout from the program"""
        
        server_cmd = "./dns_test"
        data_file = "tests/test_datafile.txt"
        
        if not os.path.exists(server_cmd):
            pytest.fail(f"DNS server executable not found: {server_cmd}")

        cmd = [server_cmd, "-p", "5353", "-s", "dns.google"]
        if data_file and os.path.exists(data_file):
            cmd.extend(["-f", data_file])
        
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Počkaj chvíľu kým sa server nastartuje
        time.sleep(2)
        
        # Teraz elegantne ukonči server pomocou SIGTERM
        proc.terminate()
        
        try:
            # Počkaj na ukončenie procesu
            stdout, stderr = proc.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            # Ak neodpovedá na SIGTERM, použi SIGKILL
            proc.kill()
            stdout, stderr = proc.communicate()

        output_lines = [line.strip() for line in stdout.splitlines() if line.strip()]

        # Výpis pre kontrolu
        for line in output_lines:
            if line[0] == "#" or line[0] == '\0':
                assert False, f"Comment or empty line found in output: {line}"
        
        

        assert output_lines, "No output captured from program"

def test_missing_data_file():
    """Test server behavior when data file is missing"""
    server_cmd = "./dns_test"
    missing_data_file = "tests/nonexistent_file.txt"
    
    # Check if server executable exists
    if not os.path.exists(server_cmd):
        pytest.skip(f"DNS server executable not found: {server_cmd}")
    
    # Make sure the file doesn't exist
    if os.path.exists(missing_data_file):
        os.remove(missing_data_file)
    
    cmd = [server_cmd, "-p", "5353", "-s", "8.8.8.8", "-f", missing_data_file]
    
    # Run the command and capture output
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=5  # Should fail quickly
    )
    
    # Server should fail with non-zero exit code
    assert result.returncode != 0, "Server should fail when data file is missing"
    
    # There should be an error message
    assert result.stderr != "", "Expected error message when data file is missing"
    
    # Error should mention file issue
    error_lower = result.stderr.lower()
    assert any(keyword in error_lower for keyword in 
              ["file", "open", "missing", "not found", "error"]), \
           f"Error message should mention file issue. Got: {result.stderr}"

def test_missing_arg_server():
    """Test server behavior when server argument is missing"""
    server_cmd = "./dns_test"
    data_file = "tests/test_datafile.txt"
    
    # Check if server executable exists
    if not os.path.exists(server_cmd):
        pytest.skip(f"DNS server executable not found: {server_cmd}")
    
    # OPRAVA: Pridaná chýbajúca čiarka
    cmd = [server_cmd, "-p", "5353", "-f", data_file]  # ← tu chýbala čiarka!

    # Run the command and capture output
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=5
    )
    
    # Server should fail with non-zero exit code
    assert result.returncode != 0, "program should fail when server argument is missing"

    # OPRAVA: Skontrolovať stdout namiesto stderr, keďže help sa vypíše do stdout
    assert result.stdout != "" or result.stderr != "", "Expected error message when server argument is missing"

    # Skontrolovať či výstup obsahuje chybovú správu alebo help
    output = result.stdout + result.stderr
    error_lower = output.lower()
    assert any(keyword in error_lower for keyword in
               ["server", "s", "argument", "missing", "error", "usage"]), \
        f"Error message should mention server issue. Got: {output}"

def test_more_arguments():
    """Test server behavior when more arguments are provided"""
    server_cmd = "./dns_test"
    data_file = "tests/test_datafile.txt"
    
    # Check if server executable exists
    if not os.path.exists(server_cmd):
        pytest.skip(f"DNS server executable not found: {server_cmd}")
    
    # OPRAVA: Pridaná chýbajúca čiarka
    cmd = [server_cmd, "-p", "5353", "-s", "8.8.8.8", "-w", "one_more_arg", "-f", data_file] 

    # Run the command and capture output
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=5
    )
    
    # Server should fail with non-zero exit code
    assert result.returncode != 0, "program should fail when more arguments are provided"

    # OPRAVA: Skontrolovať stdout namiesto stderr, keďže help sa vypíše do stdout
    assert result.stdout != "" or result.stderr != "", "Expected error message when more arguments are provided"

    # Skontrolovať či výstup obsahuje chybovú správu alebo help
    output = result.stdout + result.stderr
    error_lower = output.lower()
    assert any(keyword in error_lower for keyword in
               ["error", "usage"]), \
        f"Error message should mention server issue. Got: {output}"
    

def pytest_addoption(parser):
    """Add custom command line options"""
    parser.addoption("--server-cmd", action="store", default="./dns",
                    help="Path to DNS server executable")
    parser.addoption("--data-file", action="store", default="",
                    help="Path to data file for DNS server (optional)")
    parser.addoption("--no-start-server", action="store_true",
                    help="Use already running server instead of starting one")



# Run the tests
if __name__ == "__main__":
    pytest.main([__file__, "-v"])