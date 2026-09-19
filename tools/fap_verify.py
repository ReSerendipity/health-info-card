# tools/fap_verify.py —— 真机验收:唤醒设备后串行完成 5 页健康卡截图 + 按键翻页。
# 用法: python tools/fap_verify.py COM3 <out_dir>
import serial
import subprocess
import sys
import time

PORT = sys.argv[1]
OUT_DIR = sys.argv[2]
PY = sys.executable
TOOL = "tools/fap_device.py"

# 不做 DTR 复位:USB CDC 线路状态变化会让设备 USB 短暂重枚举,
# 干扰 fap 驱动 RX(实测复位后截图全部超时,不复位则稳定)。
# 唤醒设备请用 esptool 刷写后的 hard reset,或等设备自行唤醒。

# 3) 在 60s 深睡窗口内串行:截图页1 -> 下键 -> 截图页2 -> ...
names = ["01_name_age_blood", "02_family_phone", "03_home",
         "04_medical", "05_wechat"]
fail = 0
for i in range(5):
    out = f"{OUT_DIR}\\fap_page_{names[i]}.bmp"
    r = subprocess.run([PY, TOOL, "-p", PORT, "shot", out],
                       capture_output=True, text=True, timeout=40)
    print(f"[{i + 1}/5] shot {names[i]}: rc={r.returncode} {r.stdout.strip()[:80]}")
    if r.returncode != 0:
        fail += 1
        print("   stderr:", r.stderr.strip()[:160])
    if i < 4:
        r = subprocess.run([PY, TOOL, "-p", PORT, "key", "1", "1"],
                           capture_output=True, text=True, timeout=20)
        print(f"   key DOWN: rc={r.returncode} {r.stdout.strip()[:60]}")
        if r.returncode != 0:
            fail += 1
        time.sleep(0.4)

print("FAILURES:", fail)
sys.exit(1 if fail else 0)
