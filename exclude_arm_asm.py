"""
PlatformIO pre-build script for LVGL on ESP32.

1. Excludes ARM assembly files (helium/neon) that fail on Xtensa
2. Copies lv_conf.h to the LVGL library directory
"""

Import("env")
import os
import shutil

def setup_lvgl(env):
    """Configure LVGL library for ESP32 build."""
    libdeps_dir = env.get("PROJECT_LIBDEPS_DIR", "")
    project_dir = env.get("PROJECT_DIR", "")
    env_name = env.get("PIOENV", "")
    
    if not libdeps_dir or not env_name or not project_dir:
        return
    
    lvgl_dir = os.path.join(libdeps_dir, env_name, "lvgl")
    if not os.path.exists(lvgl_dir):
        return
    
    # 1. Remove ARM assembly files that cause issues on ESP32
    asm_dirs = [
        "src/draw/sw/blend/helium",
        "src/draw/sw/blend/neon",
        "src/draw/convert/helium",
        "src/draw/convert/neon"
    ]
    
    for asm_dir in asm_dirs:
        full_path = os.path.join(lvgl_dir, asm_dir)
        if os.path.exists(full_path):
            for filename in os.listdir(full_path):
                if filename.endswith('.S'):
                    filepath = os.path.join(full_path, filename)
                    try:
                        os.remove(filepath)
                        print(f"LVGL: Removed ARM assembly: {os.path.basename(filepath)}")
                    except OSError:
                        pass

    # 2. Copy lv_conf.h to LVGL library directory
    src_conf = os.path.join(project_dir, "include", "lv_conf.h")
    dst_conf = os.path.join(lvgl_dir, "lv_conf.h")
    
    if os.path.exists(src_conf):
        # Only copy if source is newer or dest doesn't exist
        if not os.path.exists(dst_conf) or \
           os.path.getmtime(src_conf) > os.path.getmtime(dst_conf):
            shutil.copy2(src_conf, dst_conf)
            print(f"LVGL: Copied lv_conf.h to library directory")

setup_lvgl(env)
