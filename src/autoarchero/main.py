from __future__ import annotations

import sys
from pathlib import Path

# Allow `python -m autoarchero.main` from repo without installing package
_SRC = Path(__file__).resolve().parents[1]
if str(_SRC) not in sys.path:
    sys.path.insert(0, str(_SRC))

from PySide6.QtWidgets import QApplication

from autoarchero.main_window import MainWindow


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("AutoArchero")
    win = MainWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
