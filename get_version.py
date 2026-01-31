Import("env")
import subprocess
import datetime
import os

def get_semantic_version():
    """Read semantic version from VERSION file"""
    version_file = os.path.join(env.subst("$PROJECT_DIR"), "VERSION")
    try:
        with open(version_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith("VERSION="):
                    return line.split("=", 1)[1].strip()
    except Exception:
        pass
    return "0.0.0"

def get_git_hash():
    """Get short git commit hash with dirty flag"""
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            cwd=env.subst("$PROJECT_DIR")
        )
        if result.returncode == 0:
            git_hash = result.stdout.strip()
            
            # Check if working directory is dirty
            result = subprocess.run(
                ["git", "status", "--porcelain"],
                capture_output=True,
                text=True,
                cwd=env.subst("$PROJECT_DIR")
            )
            if result.returncode == 0 and result.stdout.strip():
                git_hash += "-dirty"
            
            return git_hash
    except Exception:
        pass
    return "unknown"

def get_build_timestamp():
    """Get current build timestamp"""
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

# Get version info
semantic_version = get_semantic_version()
git_hash = get_git_hash()
full_version = f"{semantic_version}-{git_hash}"
timestamp = get_build_timestamp()

print(f"")
print(f"=== Build Info ===")
print(f"  Version: {full_version}")
print(f"  Timestamp: {timestamp}")
print(f"==================")
print(f"")

# Add build flags
env.Append(CPPDEFINES=[
    ("FIRMWARE_VERSION", env.StringifyMacro(full_version)),
    ("BUILD_TIMESTAMP", env.StringifyMacro(timestamp))
])
