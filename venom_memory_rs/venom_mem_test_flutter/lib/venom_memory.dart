/// VenomMemory FFI Bindings for Dart
///
/// This file provides Dart bindings to the VenomMemory C library.

import 'dart:ffi';
// import 'dart:io';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

// C function types
typedef VenomShellConnectNative = Pointer<Void> Function(Pointer<Utf8>);
typedef VenomShellConnect = Pointer<Void> Function(Pointer<Utf8>);

typedef VenomShellDestroyNative = Void Function(Pointer<Void>);
typedef VenomShellDestroy = void Function(Pointer<Void>);

typedef VenomShellReadDataNative =
    IntPtr Function(Pointer<Void>, Pointer<Uint8>, IntPtr);
typedef VenomShellReadData = int Function(Pointer<Void>, Pointer<Uint8>, int);

typedef VenomShellIdNative = Uint32 Function(Pointer<Void>);
typedef VenomShellId = int Function(Pointer<Void>);

/// VenomMemory Shell - connects to a daemon channel
class VenomShell {
  static DynamicLibrary? _lib;
  Pointer<Void>? _handle;

  static DynamicLibrary _loadLibrary() {
    if (_lib != null) return _lib!;

    // Try different paths to find the library
    final paths = [
      'libvenom_memory.so',
      './libvenom_memory.so',
      '../target/release/libvenom_memory.so',
      '../../target/release/libvenom_memory.so',
    ];

    for (final path in paths) {
      try {
        _lib = DynamicLibrary.open(path);
        print('✅ Loaded VenomMemory from: $path');
        return _lib!;
      } catch (e) {
        continue;
      }
    }

    throw Exception('Could not load libvenom_memory.so');
  }

  VenomShell(String channelName) {
    final lib = _loadLibrary();

    final connect = lib
        .lookupFunction<VenomShellConnectNative, VenomShellConnect>(
          'venom_shell_connect',
        );

    final namePtr = channelName.toNativeUtf8();
    _handle = connect(namePtr);
    calloc.free(namePtr);

    if (_handle == nullptr) {
      throw Exception('Failed to connect to channel: $channelName');
    }
  }

  /// Get shell client ID
  int get clientId {
    if (_handle == null) return 0;
    final lib = _loadLibrary();
    final getId = lib.lookupFunction<VenomShellIdNative, VenomShellId>(
      'venom_shell_id',
    );
    return getId(_handle!);
  }

  /// Read data from shared memory
  Uint8List readData(int maxLen) {
    if (_handle == null) return Uint8List(0);

    final lib = _loadLibrary();
    final readFn = lib
        .lookupFunction<VenomShellReadDataNative, VenomShellReadData>(
          'venom_shell_read_data',
        );

    final bufPtr = calloc<Uint8>(maxLen);
    final bytesRead = readFn(_handle!, bufPtr, maxLen);

    final result = Uint8List.fromList(bufPtr.asTypedList(bytesRead));
    calloc.free(bufPtr);

    return result;
  }

  /// Destroy the shell connection
  void dispose() {
    if (_handle == null) return;

    final lib = _loadLibrary();
    final destroy = lib
        .lookupFunction<VenomShellDestroyNative, VenomShellDestroy>(
          'venom_shell_destroy',
        );
    destroy(_handle!);
    _handle = null;
  }

  /// Send command to daemon
  bool sendCommand(String cmd) {
    if (_handle == null) return false;

    final lib = _loadLibrary();

    final sendFn = lib
        .lookupFunction<
          Uint8 Function(Pointer<Void>, Pointer<Uint8>, IntPtr),
          int Function(Pointer<Void>, Pointer<Uint8>, int)
        >('venom_shell_send_command');

    final cmdBytes = cmd.codeUnits;
    final cmdPtr = calloc<Uint8>(cmdBytes.length);
    for (int i = 0; i < cmdBytes.length; i++) {
      cmdPtr[i] = cmdBytes[i];
    }

    final result = sendFn(_handle!, cmdPtr, cmdBytes.length);
    calloc.free(cmdPtr);

    return result != 0;
  }
}

/// SystemStats struct - must match Rust daemon's struct
class SystemStats {
  final double cpuUsagePercent;
  final List<double> cpuCores;
  final int coreCount;
  final int memoryUsedMb;
  final int memoryTotalMb;
  final int uptimeSeconds;

  SystemStats({
    required this.cpuUsagePercent,
    required this.cpuCores,
    required this.coreCount,
    required this.memoryUsedMb,
    required this.memoryTotalMb,
    required this.uptimeSeconds,
  });

  /// Parse from raw bytes (matching Rust #[repr(C)] struct)
  factory SystemStats.fromBytes(Uint8List bytes) {
    if (bytes.length < 96) {
      return SystemStats(
        cpuUsagePercent: 0,
        cpuCores: List.filled(16, 0),
        coreCount: 0,
        memoryUsedMb: 0,
        memoryTotalMb: 0,
        uptimeSeconds: 0,
      );
    }

    final data = ByteData.view(bytes.buffer);

    // Parse float (4 bytes) at offset 0
    final cpuUsage = data.getFloat32(0, Endian.little);

    // Parse 16 floats (64 bytes) at offset 4
    final cores = <double>[];
    for (int i = 0; i < 16; i++) {
      cores.add(data.getFloat32(4 + i * 4, Endian.little));
    }

    // Parse uint32 at offset 68
    final coreCount = data.getUint32(68, Endian.little);

    // Parse uint32 at offset 72
    final memUsed = data.getUint32(72, Endian.little);

    // Parse uint32 at offset 76
    final memTotal = data.getUint32(76, Endian.little);

    // Parse uint64 at offset 80
    final uptime = data.getUint64(80, Endian.little);

    return SystemStats(
      cpuUsagePercent: cpuUsage,
      cpuCores: cores,
      coreCount: coreCount,
      memoryUsedMb: memUsed,
      memoryTotalMb: memTotal,
      uptimeSeconds: uptime,
    );
  }
}
