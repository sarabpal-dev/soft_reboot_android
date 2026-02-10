# Soft Reboot

A Linux kernel module that provides safe userspace reboot functionality for Android devices. Triggers a clean soft reboot by terminating zygote processes, allowing init to respawn the Android framework without requiring a full device reboot.

**Especially useful for ROMs that don't natively support userspace reboot** - this module adds the functionality at the kernel level.

Perfect for KernelSU/Magisk/LSPosed module injection scenarios where you need to reload userspace without kernel restart.

## Usage
```bash
echo 1 > /dev/soft_reboot
```

## Installation
```bash
# Load the module
insmod soft_reboot.ko

# Trigger soft reboot
echo 1 > /dev/soft_reboot
```

## How It Works

The module creates a `/dev/soft_reboot` character device. When triggered, it sends SIGKILL to zygote processes, causing Android's init system to respawn them along with:
- system_server
- All Android apps
- Framework services
