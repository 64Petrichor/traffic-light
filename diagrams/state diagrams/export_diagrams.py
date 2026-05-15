#!/usr/bin/env python3
"""Export .mmd diagrams to PNG."""

import subprocess
import sys
from pathlib import Path

# On Windows, npx is a .cmd script and needs shell=True or the explicit extension
NPX = "npx.cmd" if sys.platform == "win32" else "npx"

DIAGRAMS = Path(__file__).parent


def export_mmd():
    mmd_files = sorted(DIAGRAMS.glob("*.mmd"))
    if not mmd_files:
        print("No .mmd files found.")
        return

    for mmd in mmd_files:
        out = mmd.with_suffix(".png")
        print(f"  {mmd.name} -> {out.name}")
        result = subprocess.run(
            [NPX, "--yes", "@mermaid-js/mermaid-cli",
             "-i", str(mmd), "-o", str(out), "-b", "white"],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"    ERROR:\n{result.stderr.strip()}")
            sys.exit(result.returncode)

    print(f"Exported {len(mmd_files)} diagram(s).")


if __name__ == "__main__":
    print("=== Exporting Mermaid diagrams ===")
    export_mmd()
    print(
        "\nNote: ML figures (benchmark.png, rl_learning_curve.png, etc.) are now"
        " saved directly to diagrams/graphs/ by the Jupyter notebook."
        " Run the notebook to regenerate them."
    )
    print("\nDone. All files are in diagrams/")
