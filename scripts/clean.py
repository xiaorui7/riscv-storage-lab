"""Remove only generated files in the project's dedicated build directory."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
target = root / "build"
if target.is_symlink() or target.resolve().parent != root:
    raise SystemExit("Refusing to clean a build path outside the repository")
if target.exists():
    shutil.rmtree(target)
