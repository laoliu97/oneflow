# python3 -m pip install astpretty
# requires python3.9 to run
import ast
import os
import argparse
import ast
import subprocess
import multiprocessing
from pathlib import Path
import astpretty
import sys
from astpretty import pprint

parser = argparse.ArgumentParser()
parser.add_argument(
    "--out_dir", type=str, default="python",
)
parser.add_argument("--verbose", "-v", action="store_true")
ONEFLOW_TEST_PYTORCH_VISION_DIR = os.getenv("ONEFLOW_TEST_PYTORCH_VISION_DIR")
parser.add_argument(
    "--pytorch_vision_dir", type=str, default=ONEFLOW_TEST_PYTORCH_VISION_DIR,
)
args = parser.parse_args()

ONEFLOW_TEST_PYTORCH_VISION_PATH = Path(args.pytorch_vision_dir)


class CompatibilityVisitor(ast.NodeVisitor):
    from ast import ImportFrom, Import

    def __init__(self) -> None:
        super().__init__()

    def visit_ImportFrom(self, node: ImportFrom):
        if node.module:
            if node.module == "torch" or "torch." in node.module:
                for a in node.names:
                    assert isinstance(a, ast.alias)
                    pprint(a)

    def visit_Import(self, node: Import):
        for a in node.names:
            assert isinstance(a, ast.alias)
            if a.name.startswith("torch") and a.name != "torch":
                pprint(a.name)


def analyze_py(args):
    src: Path = args["src"]
    tree = ast.parse(src.read_text())
    v = CompatibilityVisitor()
    v.visit(tree)


if __name__ == "__main__":
    print(ONEFLOW_TEST_PYTORCH_VISION_PATH)
    py_srcs = ONEFLOW_TEST_PYTORCH_VISION_PATH.glob("**/*.py")
    pool = multiprocessing.Pool()
    pool.map(
        analyze_py, [{"src": src,} for src in py_srcs],
    )
    pool.close()
