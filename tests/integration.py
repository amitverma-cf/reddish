"""Process-level WAL, capacity, protocol and optional Redis differential checks."""
import argparse
import os
from pathlib import Path
import random
import struct
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('binary'); parser.add_argument('--redis', action='store_true')
options = parser.parse_args()

def request(*args):
    args = [str(a).encode() if not isinstance(a, bytes) else a for a in args]
    return b'*%d\r\n' % len(args) + b''.join(b'$%d\r\n' % len(a) + a + b'\r\n' for a in args)

class Process:
    def __init__(self, root, flags=(), redis=False):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0)); self.port = sock.getsockname()[1]
        self.log = open(Path(root) / f'{self.port}.log', 'wb')
        args = ['redis-server', '--port', str(self.port), '--save', '', '--appendonly', 'no'] if redis else [options.binary, str(self.port), *flags]
        self.process = subprocess.Popen(args, stdout=self.log, stderr=self.log)
        self.sock = None
        for _ in range(200):
            if self.process.poll() is not None: raise RuntimeError(f'server exited; see {self.log.name}')
            try:
                self.sock = socket.create_connection(('127.0.0.1', self.port), timeout=5); break
            except OSError: time.sleep(.01)
        if self.sock is None: self.close(); raise RuntimeError('startup timed out')
        self.stream = self.sock.makefile('rb')
    def reply(self):
        line = self.stream.readline()
        if not line: raise EOFError('connection closed')
        if line.startswith(b'$'):
            n = int(line[1:])
            if n < 0: return None
            value = self.stream.read(n)
            if len(value) != n or self.stream.read(2) != b'\r\n': raise EOFError('incomplete bulk reply')
            return value
        if line.startswith(b':'): return int(line[1:])
        if line.startswith(b'+'): return line[1:-2]
        if line.startswith(b'-'): return ('error', line[1:-2])
        raise RuntimeError(f'invalid reply: {line!r}')
    def command(self, *args): self.sock.sendall(request(*args)); return self.reply()
    def close(self, crash=False):
        if self.sock is not None: self.stream.close(); self.sock.close(); self.sock = None
        if self.process.poll() is None:
            if crash: self.process.kill()
            else: self.process.terminate()
            try: self.process.wait(timeout=10)
            except subprocess.TimeoutExpired: self.process.kill(); self.process.wait()
        self.log.close()

class Integration(unittest.TestCase):
    def setUp(self): self.temp = tempfile.TemporaryDirectory(); self.servers = []
    def tearDown(self):
        for server in self.servers: server.close()
        self.temp.cleanup()
    def start(self, flags=('--no-wal',), redis=False):
        server = Process(self.temp.name, flags, redis); self.servers.append(server); return server
    def test_capacity_and_expiry(self):
        server = self.start(('--no-wal', '--max-keys', '1'))
        self.assertEqual(server.command('SET', 'k', 'v'), b'OK'); time.sleep(.1)
        self.assertEqual(server.command('GET', 'k'), b'v')
        self.assertEqual(server.command('SET', 'new', 'v'), b'OK')
        self.assertIsNone(server.command('GET', 'k')); self.assertEqual(server.command('GET', 'new'), b'v')
        self.assertEqual(server.command('EXPIRE', 'new', 1), 1); time.sleep(1.1)
        self.assertIsNone(server.command('GET', 'new'))
        self.assertIn(b'keys:0', server.command('INFO'))
    def test_deep_pipeline_half_close_and_binary_data(self):
        server = self.start()
        data = b'a\x00b\r\n' * 20
        self.assertEqual(server.command('SET', 'k', data), b'OK')
        server.sock.sendall(request('GET', 'k') * 2500)
        server.sock.shutdown(socket.SHUT_WR)
        for _ in range(2500): self.assertEqual(server.reply(), data)
        self.assertEqual(server.stream.read(1), b'')
    def test_malformed_terminator_closes_promptly(self):
        server = self.start(); server.sock.sendall(request('PING')[:-2] + b'xx')
        response = server.reply(); self.assertEqual(response[0], 'error')
        self.assertEqual(server.stream.read(1), b'')
    def test_large_hash_and_reply_limit(self):
        server = self.start(); value = b'x' * 70000
        self.assertEqual(server.command('HSET', 'h', 'f', value), 1)
        self.assertEqual(server.command('HGET', 'h', 'f'), value)
        self.assertEqual(server.command('HDEL', 'h', 'f'), 1)
        self.assertEqual(server.command('SET', 'big', b'x' * 262144), b'OK')
        server.sock.sendall(request('GET', 'big') * 100)
        try: self.assertEqual(server.stream.read(1), b'')
        except ConnectionResetError: pass
        # One oversized client must not take down the server.
        with socket.create_connection(('127.0.0.1', server.port), timeout=5) as sock:
            sock.sendall(request('PING')); self.assertEqual(sock.recv(100), b'+PONG\r\n')
    def test_durable_wal_crash_recovery_and_offline_ttl(self):
        wal = str(Path(self.temp.name) / 'data.wal')
        flags = ('--wal-path', wal, '--wal-sync', 'always', '--max-keys', '100')
        server = self.start(flags)
        self.assertEqual(server.command('SET', 'persist', 'v'), b'OK')
        self.assertEqual(server.command('RPUSH', 'list', 'one', 'two'), 2)
        self.assertEqual(server.command('LPOP', 'list'), b'one')
        self.assertEqual(server.command('HSET', 'h', 'f', b'x' * 70000), 1)
        self.assertEqual(server.command('SET', 'ttl', 'gone'), b'OK')
        self.assertEqual(server.command('EXPIRE', 'ttl', 1), 1)
        server.sock.sendall(b''.join(request('SET', f'batch{i}', i) for i in range(20)))
        for _ in range(20): self.assertEqual(server.reply(), b'OK')
        server.close(crash=True); time.sleep(1.1)
        restored = self.start(flags)
        self.assertEqual(restored.command('GET', 'persist'), b'v')
        self.assertEqual(restored.command('LPOP', 'list'), b'two')
        self.assertEqual(restored.command('HGET', 'h', 'f'), b'x' * 70000)
        self.assertIsNone(restored.command('GET', 'ttl'))
        for i in range(20): self.assertEqual(restored.command('GET', f'batch{i}'), str(i).encode())
    def test_offline_snapshot_migration(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('importer', Path(__file__).resolve().parents[1] / 'benchmarks/migrate_snapshot.py')
        importer = importlib.util.module_from_spec(spec); spec.loader.exec_module(importer)
        def text(value): return struct.pack('<Q', len(value)) + value
        for version in (1, 3):
            source, wal = Path(self.temp.name) / f'v{version}.dump', Path(self.temp.name) / f'v{version}.wal'
            data = struct.pack('<QQ', version, 2)
            deadline = struct.pack('<q', -1 if version == 1 else 0)
            data += text(b'counter') + deadline + b'\x01' + struct.pack('<q', 42)
            data += text(b'list') + deadline + b'\x02' + struct.pack('<Q', 2) + b'\x00' + text(b'one') + b'\x01' + struct.pack('<q', 7)
            source.write_bytes(data); importer.migrate(source, wal)
            self.assertEqual(source.read_bytes(), data)
            with self.assertRaises(FileExistsError): importer.migrate(source, wal)
            server = self.start(('--wal-path', str(wal)))
            self.assertEqual(server.command('INCR', 'counter'), 43)
            self.assertEqual(server.command('LPOP', 'list'), b'one')
            self.assertEqual(server.command('LPOP', 'list'), b'7')
    @unittest.skipUnless(options.redis and shutil.which('redis-server'), 'optional Redis differential comparison')
    def test_differential_commands(self):
        target, baseline = self.start(), self.start(redis=True)
        rng = random.Random(731)
        for _ in range(2000):
            n = rng.randrange(20); value = rng.randbytes(rng.randrange(150))
            commands = [('SET', f's{n}', value), ('GET', f's{n}'), ('INCR', f'i{n}'), ('DEL', f's{n}'),
                        ('LPUSH', f'l{n}', value), ('RPUSH', f'l{n}', value), ('LPOP', f'l{n}'), ('RPOP', f'l{n}'), ('LLEN', f'l{n}'),
                        ('HSET', f'h{n}', f'f{rng.randrange(15)}', value), ('HGET', f'h{n}', 'f3'), ('HDEL', f'h{n}', 'f3'), ('HLEN', f'h{n}')]
            command = rng.choice(commands)
            self.assertEqual(target.command(*command), baseline.command(*command), command)

unittest.main(argv=[__file__], verbosity=2)
