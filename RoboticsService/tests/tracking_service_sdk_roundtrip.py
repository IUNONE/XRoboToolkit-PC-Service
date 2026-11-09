"""Exercise the real service, native C SDK and PICO TCP framing without a headset.

Only Python's standard library is required. Ports, configuration, locks and logs
are isolated from an installed/running service.
"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import threading
import time


def unused_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def packet(command, payload):
    return struct.pack('<BBI', 0x3F, command, len(payload)) + payload + struct.pack('<QB', int(time.time()), 0xA5)


class DeviceStateJson(ctypes.Structure):
    _fields_ = [('device_id', ctypes.c_char * 32), ('state_json', ctypes.c_char * 16352)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service', required=True, type=Path)
    parser.add_argument('--sdk', required=True, type=Path)
    args = parser.parse_args()
    service = args.service.resolve()
    library = args.sdk.resolve()
    tcp_port, grpc_port, broadcast_port = unused_port(), unused_port(), unused_port()
    assert len({tcp_port, grpc_port, broadcast_port}) == 3
    sdk = ctypes.CDLL(str(library))
    callback_type = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
    sdk.PXREAInit.argtypes = [ctypes.c_void_p, callback_type, ctypes.c_uint]
    sdk.PXREAInit.restype = ctypes.c_int
    sdk.PXREADeinit.restype = ctypes.c_int
    sdk.PXREADeviceControlJson.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    sdk.PXREADeviceControlJson.restype = ctypes.c_int
    connected = threading.Event()
    state_received = threading.Event()
    received = []
    expected_state = {'functionName': 'Tracking', 'value': json.dumps({
        'Controller': {'left': {'pose': '0.1,0.2,0.3,0,0,0,1', 'axisX': 0.25,
                                'axisY': -0.5, 'trigger': 0.7, 'grip': 0.8}},
        'timeStampNs': 123456789})}

    @callback_type
    def callback(context, event, status, data):
        if event == (1 << 2):
            connected.set()
        if event == (1 << 25):
            state = ctypes.cast(data, ctypes.POINTER(DeviceStateJson)).contents
            received.append((state.device_id.decode(), json.loads(state.state_json.decode())))
            state_received.set()

    with tempfile.TemporaryDirectory(prefix='xrt-native-test-') as directory:
        directory = Path(directory)
        config = directory / 'setting.ini'
        config.write_text(f'[Log]\nlogToFile=false\n[Service]\nlistenAddr=127.0.0.1\nlistenPort={grpc_port}\n'
                          f'[TCP]\nTcpBindPort={tcp_port}\nBroadCastSendPort={broadcast_port}\n')
        sdk_config = directory / 'sdk.ini'
        sdk_config.write_text(f'[Client]\nconnectAddr=127.0.0.1\nconnectPort={grpc_port}\n')
        environment = dict(os.environ, XRT_SERVICE_CONFIG_FILE=str(config), XRT_RUNTIME_DIRECTORY=str(directory / 'runtime'))
        previous_sdk_config = os.environ.get('XRT_SDK_CONFIG_FILE')
        os.environ['XRT_SDK_CONFIG_FILE'] = str(sdk_config)
        initialized = False
        client = None
        log_path = directory / 'service.log'
        with log_path.open('wb') as log:
            process = subprocess.Popen([str(service)], cwd=directory, env=environment, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 10
                while True:
                    if process.poll() is not None:
                        raise AssertionError('Service exited before listening')
                    try:
                        client = socket.create_connection(('127.0.0.1', tcp_port), timeout=.1)
                        break
                    except OSError:
                        if time.monotonic() > deadline:
                            raise AssertionError('Service did not start its TCP listener')
                        time.sleep(.05)
                assert sdk.PXREAInit(None, callback, 0xffffffff) == 0
                initialized = True
                assert connected.wait(5), 'Native SDK did not connect through gRPC'
                # The transport discovers the serial from this first device packet.
                client.sendall(packet(0x19, b'mac-test-headset|1'))
                time.sleep(.1)
                client.sendall(packet(0x6D, json.dumps(expected_state).encode()))
                assert state_received.wait(5), 'PICO tracking JSON did not reach the native SDK'
                assert received[-1] == ('mac-test-headset', expected_state), received
                # Verify SDK -> gRPC -> original PICO TCP control protocol as well.
                control = b'{"functionName":"Tracking","value":"test"}'
                assert sdk.PXREADeviceControlJson(b'mac-test-headset', control) == 0
                client.settimeout(3)
                response = b''
                while control not in response:
                    chunk = client.recv(4096)
                    if not chunk:
                        raise AssertionError('PICO connection closed before control reply')
                    response += chunk
                assert b'\xcf\x5f' in response, response
                # A second service in the same runtime directory must exit cleanly.
                second = subprocess.run([str(service)], cwd=directory, env=environment,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=5)
                assert second.returncode == 0
                assert sdk.PXREADeinit() == 0
                initialized = False
                assert sdk.PXREADeinit() == 0  # idempotent cleanup
                print('PASS: service startup, native SDK/gRPC, PICO JSON, outbound control and single-instance behavior')
            except BaseException:
                log.flush()
                print(log_path.read_text(errors='replace'))
                raise
            finally:
                if client is not None:
                    client.close()
                if initialized:
                    sdk.PXREADeinit()
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
                if previous_sdk_config is None:
                    os.environ.pop('XRT_SDK_CONFIG_FILE', None)
                else:
                    os.environ['XRT_SDK_CONFIG_FILE'] = previous_sdk_config


if __name__ == '__main__':
    main()
