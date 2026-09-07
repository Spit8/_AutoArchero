from __future__ import annotations

import shutil
import subprocess
from pathlib import Path
from typing import List, Optional


DEFAULT_ADB_CANDIDATES = [
    Path(r"C:\Program Files\BlueStacks_nxt\HD-Adb.exe"),
    Path(r"C:\Program Files\BlueStacks\HD-Adb.exe"),
]


class AdbError(RuntimeError):
    pass


class AdbClient:
    """BlueStacks / Android ADB helper. Inputs are ghost (no Windows cursor)."""

    def __init__(self, adb_path: Optional[str] = None, serial: Optional[str] = None) -> None:
        self.adb_path = self._resolve_adb(adb_path)
        self.serial = serial

    @staticmethod
    def _resolve_adb(adb_path: Optional[str]) -> str:
        if adb_path:
            p = Path(adb_path)
            if not p.is_file():
                raise AdbError(f"adb not found: {adb_path}")
            return str(p)
        for cand in DEFAULT_ADB_CANDIDATES:
            if cand.is_file():
                return str(cand)
        which = shutil.which("adb")
        if which:
            return which
        raise AdbError("No BlueStacks HD-Adb.exe / adb found")

    def _base_cmd(self) -> List[str]:
        cmd = [self.adb_path]
        if self.serial:
            cmd += ["-s", self.serial]
        return cmd

    def run(self, *args: str, timeout: float = 30.0, check: bool = True) -> subprocess.CompletedProcess:
        cmd = self._base_cmd() + list(args)
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise AdbError(f"adb timeout: {' '.join(cmd)}") from exc
        if check and proc.returncode != 0:
            err = (proc.stderr or b"").decode("utf-8", errors="replace").strip()
            raise AdbError(f"adb failed ({proc.returncode}): {err or ' '.join(cmd)}")
        return proc

    def devices(self) -> List[str]:
        proc = self.run("devices", check=True)
        lines = (proc.stdout or b"").decode("utf-8", errors="replace").strip().splitlines()
        out: List[str] = []
        for line in lines[1:]:
            parts = line.split()
            if len(parts) >= 2 and parts[1] == "device":
                out.append(parts[0])
        return out

    def ensure_device(self) -> str:
        if self.serial:
            return self.serial
        devs = self.devices()
        if not devs:
            raise AdbError("No ADB device online (start BlueStacks)")
        if len(devs) == 1:
            self.serial = devs[0]
            return self.serial
        # Prefer classic emulator serial
        for d in devs:
            if d.startswith("emulator-"):
                self.serial = d
                return self.serial
        self.serial = devs[0]
        return self.serial

    def screencap_png(self) -> bytes:
        self.ensure_device()
        proc = self.run("exec-out", "screencap", "-p", timeout=60.0)
        data = proc.stdout or b""
        if len(data) < 100:
            raise AdbError("screencap returned empty data")
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            # Some adb clients expand LF→CRLF inside the binary stream
            fixed = data.replace(b"\r\n", b"\n")
            if fixed[:8] == b"\x89PNG\r\n\x1a\n":
                return fixed
            raise AdbError("screencap did not return a PNG")
        return data

    def tap(self, x: int, y: int) -> None:
        self.ensure_device()
        self.run("shell", "input", "tap", str(int(x)), str(int(y)))

    def swipe(self, x1: int, y1: int, x2: int, y2: int, duration_ms: int = 300) -> None:
        self.ensure_device()
        self.run(
            "shell",
            "input",
            "swipe",
            str(int(x1)),
            str(int(y1)),
            str(int(x2)),
            str(int(y2)),
            str(int(duration_ms)),
        )

    def wm_size(self) -> tuple[int, int]:
        self.ensure_device()
        proc = self.run("shell", "wm", "size")
        text = (proc.stdout or b"").decode("utf-8", errors="replace")
        # Physical size: 1920x1080
        for token in text.replace("\r", "\n").split():
            if "x" in token and token[0].isdigit():
                w, h = token.split("x", 1)
                return int(w), int(h)
        raise AdbError(f"cannot parse wm size: {text}")
