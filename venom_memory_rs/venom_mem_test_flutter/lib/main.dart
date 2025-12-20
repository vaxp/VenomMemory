import 'dart:async';
import 'package:flutter/material.dart';
import 'venom_memory.dart';

void main() {
  runApp(const VenomMonitorApp());
}

class VenomMonitorApp extends StatelessWidget {
  const VenomMonitorApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'VenomMemory Flutter Monitor',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        primaryColor: Colors.deepPurple,
        colorScheme: ColorScheme.dark(
          primary: Colors.deepPurple,
          secondary: Colors.purpleAccent,
        ),
      ),
      home: const SystemMonitorPage(),
    );
  }
}

class SystemMonitorPage extends StatefulWidget {
  const SystemMonitorPage({super.key});

  @override
  State<SystemMonitorPage> createState() => _SystemMonitorPageState();
}

class _SystemMonitorPageState extends State<SystemMonitorPage> {
  VenomShell? _shell;
  SystemStats _stats = SystemStats(
    cpuUsagePercent: 0,
    cpuCores: List.filled(16, 0),
    coreCount: 0,
    memoryUsedMb: 0,
    memoryTotalMb: 0,
    uptimeSeconds: 0,
  );
  Timer? _timer;
  int _frame = 0;
  String _error = '';
  bool _connected = false;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  void _connect() {
    try {
      _shell = VenomShell('system_monitor');
      _connected = true;
      _error = '';
      _startPolling();
    } catch (e) {
      _connected = false;
      _error = e.toString();
    }
    setState(() {});
  }

  void _startPolling() {
    _timer = Timer.periodic(const Duration(milliseconds: 100), (_) {
      if (_shell != null) {
        try {
          final bytes = _shell!.readData(256);
          final stats = SystemStats.fromBytes(bytes);
          setState(() {
            _stats = stats;
            _frame++;
          });
        } catch (e) {
          // Ignore read errors
        }
      }
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    _shell?.dispose();
    super.dispose();
  }

  String _formatUptime(int seconds) {
    final d = seconds ~/ 86400;
    final h = (seconds % 86400) ~/ 3600;
    final m = (seconds % 3600) ~/ 60;
    if (d > 0) return '${d}d ${h}h ${m}m';
    if (h > 0) return '${h}h ${m}m';
    return '${m}m';
  }

  Color _getBarColor(double percent) {
    if (percent > 80) return Colors.red;
    if (percent > 50) return Colors.orange;
    return Colors.green;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('🖥️ VenomMemory Flutter Monitor'),
        centerTitle: true,
        backgroundColor: Colors.deepPurple.shade900,
      ),
      body: _connected ? _buildMonitor() : _buildError(),
    );
  }

  Widget _buildError() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.error_outline, size: 64, color: Colors.red),
          const SizedBox(height: 16),
          const Text(
            '❌ Failed to connect to VenomMemory',
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          Text(_error, style: const TextStyle(color: Colors.grey)),
          const SizedBox(height: 16),
          const Text('Run: cargo run --release --example system_daemon'),
          const SizedBox(height: 16),
          ElevatedButton(onPressed: _connect, child: const Text('Retry')),
        ],
      ),
    );
  }

  Widget _buildMonitor() {
    final ramPercent = _stats.memoryTotalMb > 0
        ? (_stats.memoryUsedMb / _stats.memoryTotalMb * 100)
        : 0.0;

    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          // Status bar
          Card(
            color: Colors.deepPurple.shade800,
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Text(
                    'Frame: $_frame',
                    style: const TextStyle(fontWeight: FontWeight.bold),
                  ),
                  Text('Cores: ${_stats.coreCount}'),
                  Text('⏱️ ${_formatUptime(_stats.uptimeSeconds)}'),
                ],
              ),
            ),
          ),
          const SizedBox(height: 16),

          // CPU Total
          const Text(
            'CPU Total',
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          _buildProgressBar(
            _stats.cpuUsagePercent,
            '${_stats.cpuUsagePercent.toStringAsFixed(1)}%',
          ),
          const SizedBox(height: 16),

          // Per-core
          const Text(
            'Per-Core Usage',
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          ...List.generate(
            _stats.coreCount.clamp(0, 16),
            (i) => Padding(
              padding: const EdgeInsets.only(bottom: 4),
              child: _buildProgressBar(
                _stats.cpuCores[i],
                'Core $i: ${_stats.cpuCores[i].toStringAsFixed(1)}%',
                height: 20,
              ),
            ),
          ),
          const SizedBox(height: 16),

          // RAM
          const Text(
            'Memory',
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          _buildProgressBar(
            ramPercent,
            '${_stats.memoryUsedMb} / ${_stats.memoryTotalMb} MB (${ramPercent.toStringAsFixed(0)}%)',
          ),
          const SizedBox(height: 24),

          // Command Button
          Center(
            child: ElevatedButton.icon(
              onPressed: () {
                final success = _shell?.sendCommand('FAKE_100') ?? false;
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(
                    content: Text(
                      success
                          ? '✅ Command sent: FAKE_100'
                          : '❌ Failed to send command',
                    ),
                    duration: const Duration(seconds: 2),
                  ),
                );
              },
              icon: const Icon(Icons.science),
              label: const Text('Test: Fake 100% CPU (5s)'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.red.shade700,
                padding: const EdgeInsets.symmetric(
                  horizontal: 24,
                  vertical: 12,
                ),
              ),
            ),
          ),
          const SizedBox(height: 16),

          // Footer
          Center(
            child: Text(
              '🔗 Reading from Rust Daemon via VenomMemory FFI',
              style: TextStyle(color: Colors.grey.shade400, fontSize: 12),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildProgressBar(double percent, String label, {double height = 24}) {
    final clampedPercent = percent.clamp(0.0, 100.0);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: const TextStyle(fontSize: 12)),
        const SizedBox(height: 4),
        ClipRRect(
          borderRadius: BorderRadius.circular(4),
          child: LinearProgressIndicator(
            value: clampedPercent / 100,
            minHeight: height,
            backgroundColor: Colors.grey.shade800,
            valueColor: AlwaysStoppedAnimation(_getBarColor(clampedPercent)),
          ),
        ),
      ],
    );
  }
}
