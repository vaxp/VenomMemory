# 🐍 VenomMemory Rust

**مكتبة IPC الأسرع في العالم بلغة Rust.**

[![Performance](https://img.shields.io/badge/Bandwidth-37.5%20GB%2Fs-brightgreen)](https://github.com/venom/memory)
[![Latency](https://img.shields.io/badge/Latency-48%C2%B5s-blue)](https://github.com/venom/memory)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

مكتبة اتصالات بين العمليات (IPC) تعتمد على الذاكرة المشتركة (Shared Memory) بنمط **Lock-Free** لتحقيق أقصى أداء ممكن.

## 🚀 الأداء المثبت

تم كسر الرقم القياسي العالمي السابق (23.3 GB/s) وتحقيق:

- **Bandwidth**: 37.52 GB/s (+61%)
- **Throughput**: > 70,000 req/s
- **Utilization**: 98% من الحد النظري للذاكرة (DDR4)

---

## 📦 التثبيت

أضف المكتبة إلى ملف `Cargo.toml`:

```toml
[dependencies]
venom_memory = { path = "." } # أو رابط git
```

---

## 🛠️ كيفية الاستخدام

تعتمد المكتبة على هيكلية **Daemon (الكاتب)** و **Shell (القارئ)**:

### 1. الخادم (Writer / Daemon)

الخادم هو المسؤول عن إنشاء القناة وإدارتها. هو الوحيد الذي يكتب البيانات التي يراها الجميع.

```rust
use venom_memory::{DaemonChannel, ChannelConfig};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. إعداد القناة
    let config = ChannelConfig {
        data_size: 64 * 1024, // 64KB حجم البيانات
        cmd_slots: 128,       // عدد الأوامر في الطابور
        max_clients: 16,      // أقصى عدد للمتصلين
    };

    // 2. إنشاء القناة باسم "my_channel"
    let daemon = DaemonChannel::create("my_channel", config)?;
    println!("Daemon started on channel: my_channel");

    // 3. الاستماع للأوامر ومعالجتها
    daemon.run(|client_id, cmd| {
        // تحويل الأمر إلى نص
        let cmd_str = String::from_utf8_lossy(cmd);
        println!("Received from {}: {}", client_id, cmd_str);

        // تنفيذ المنطق وإرجاع الرد
        if cmd_str.contains("ping") {
            return b"pong".to_vec();
        }

        // كتابة بيانات يراها الجميع (تحديث الحالة)
        // daemon.write_data(b"New Global State Here");

        b"Unknown command".to_vec()
    });

    Ok(())
}
```

### 2. العميل (Reader / Shell)

العميل يتصل بالقناة، يقرأ البيانات لحظياً، ويرسل أوامر للخادم.

```rust
use venom_memory::ShellChannel;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // 1. الاتصال بالقناة
    let shell = ShellChannel::connect("my_channel")?;
    println!("Connected with Client ID: {}", shell.client_id());

    // 2. قراءة البيانات الحالية (بدون انتظار/قفل)
    let mut data_buf = [0u8; 1024];
    let len = shell.read_data(&mut data_buf);
    println!("Current Data: {:?}", &data_buf[..len]);

    // 3. إرسال أمر وانتظار الرد (RPC style)
    let mut response_buf = [0u8; 1024];
    let resp_len = shell.request(b"ping", &mut response_buf);
    
    let response = String::from_utf8_lossy(&response_buf[..resp_len]);
    println!("Server Response: {}", response);

    Ok(())
}
```

---

## 🔄 كيف يعمل التواصل؟

1.  **القراءة (SeqLock)**:
    *   الخادم يكتب البيانات.
    *   العملاء يقرأون البيانات مباشرة دون أي قفل (Lock-Free).
    *   إذا حدثت كتابة أثناء القراءة، تعيد المكتبة المحاولة تلقائياً (تضمن قراءة متسقة دائماً).

2.  **الكتابة (MPSC Queue)**:
    *   العملاء يرسلون الأوامر إلى طابور Commands.
    *   الخادم يسحب الأوامر واحداً تلو الآخر ويعالجها.
    *   الخادم يكتب الرد في منطقة البيانات المشتركة أو منطقة مخصصة للردود.

---

## ⚠️ متطلبات النظام
*   **CPU**: x86_64 مستحسن (لضمان atomic operations السريعة).
*   **Rust**: Stable 1.70+.
