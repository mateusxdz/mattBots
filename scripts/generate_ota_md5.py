Import("env")
import hashlib
import os
import shutil

def generate_ota_artifacts(source, target, env):
    firmware_path = str(target[0])
    if not os.path.isfile(firmware_path):
        return

    with open(firmware_path, "rb") as f:
        md5_hash = hashlib.md5(f.read()).hexdigest().lower()

    ota_dir = os.path.join(env.subst("$PROJECT_DIR"), "ota")
    os.makedirs(ota_dir, exist_ok=True)

    dst_bin = os.path.join(ota_dir, "firmware.bin")
    dst_md5 = os.path.join(ota_dir, "firmware.md5")

    shutil.copy2(firmware_path, dst_bin)
    with open(dst_md5, "w", encoding="utf-8") as f:
        f.write(md5_hash + "\n")

    print(f"[OTA-ARTIFACTS] Gerado ota/firmware.bin e ota/firmware.md5 (MD5: {md5_hash})")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", generate_ota_artifacts)

