import ast
import sys

for f in sys.argv[1:]:
    try:
        with open(f, "r", encoding="utf-8") as fh:
            ast.parse(fh.read(), filename=f)
        print("OK:", f)
    except SyntaxError as e:
        print("FAIL:", f, "->", e)
        sys.exit(1)