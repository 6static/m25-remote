"""Build enclosure-site/assets from enclosure/out, optionally regenerate the enclosure first and serve the site.

    python enclosure-site/build.py              # build changed assets only
    python enclosure-site/build.py --serve      # build, then serve and open the browser
    python enclosure-site/build.py --regen      # run enclosure/enclosure.py first
"""
import argparse
import functools
import http.server
import re
import shutil
import subprocess
import sys
import webbrowser
from pathlib import Path

import cadquery as cq

SITE = Path(__file__).resolve().parent
ENCLOSURE = SITE.parent / "enclosure"
SRC = ENCLOSURE / "out"
DST = SITE / "assets"


def stale(src, *dsts):
    return any(not d.exists() or d.stat().st_mtime < src.stat().st_mtime for d in dsts)


def fix_svg(text, pad=0.06):
    # CadQuery writes no viewBox and doesn't centre the drawing; fit the viewBox to the actual lines
    m = re.search(r'scale\(([-\d.e]+),\s*([-\d.e]+)\)\s*translate\(([-\d.e]+),\s*([-\d.e]+)\)', text)
    sx, sy, tx, ty = map(float, m.groups())
    pts = [(float(x), float(y)) for x, y in re.findall(r'([-\d.e]+),([-\d.e]+)', " ".join(re.findall(r' d="([^"]+)"', text)))]
    xs = [sx * (x + tx) for x, _ in pts]
    ys = [sy * (y + ty) for _, y in pts]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    aspect = 1 if 'width="400.0"' in text else 5 / 7  # parts are square, stacks portrait
    w = max(x1 - x0, (y1 - y0) * aspect) * (1 + 2 * pad)
    h = w / aspect
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    text = re.sub(r'width="[\d.]+"\s*height="[\d.]+"', f'viewBox="{cx - w / 2:.2f} {cy - h / 2:.2f} {w:.2f} {h:.2f}"', text, count=1)
    # heavier lines read better at page size
    return re.sub(r'stroke-width="([\d.]+)"', lambda m: f'stroke-width="{float(m.group(1)) * 1.3}"', text, count=1)


def build():
    (DST / "parts").mkdir(parents=True, exist_ok=True)
    (DST / "stacks").mkdir(parents=True, exist_ok=True)
    for f in SRC.glob("preview_*.svg"):
        if stale(f, DST / "parts" / f.name):
            (DST / "parts" / f.name).write_text(fix_svg(f.read_text()))
    for f in [*SRC.glob("module_*.stl"), SRC / "lid_ky023.stl"]:
        if stale(f, DST / "parts" / f.name):
            shutil.copy2(f, DST / "parts" / f.name)

    for step in SRC.glob("assembly_*.step"):
        name = step.stem.removeprefix("assembly_")
        if not stale(step, *(DST / "stacks" / f"{name}_{k}.stl" for k in ("shell", "inside", "esp"))):
            continue
        asm = cq.Assembly.importStep(str(step))
        groups = {"shell": [], "inside": [], "esp": []}
        for _, child in asm.traverse():
            if child.obj is None:
                continue
            shapes = child.obj.vals() if isinstance(child.obj, cq.Workplane) else [child.obj]
            if "esp32" in child.name:
                target = groups["esp"]
            elif "button" in child.name or "envelope" in child.name:
                target = groups["inside"]
            else:
                target = groups["shell"]
            for s in shapes:
                target.append(s.moved(child.loc) if child.loc else s)
        for label, solids in groups.items():
            if solids:
                cq.exporters.export(cq.Compound.makeCompound(solids), str(DST / "stacks" / f"{name}_{label}.stl"),
                                    tolerance=0.05, angularTolerance=0.2)
        print(f"stack {name}: {', '.join(f'{len(v)} {k}' for k, v in groups.items())}")


def serve(port):
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(SITE))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as httpd:
        url = f"http://localhost:{port}/"
        print(f"serving {url}  (Ctrl+C to stop)")
        webbrowser.open(url)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--regen", action="store_true", help="run enclosure/enclosure.py before building")
    ap.add_argument("--serve", action="store_true", help="serve the site and open it in the browser")
    ap.add_argument("--port", type=int, default=8765)
    args = ap.parse_args()
    if args.regen:
        subprocess.run([sys.executable, str(ENCLOSURE / "enclosure.py")], check=True)
    build()
    if args.serve:
        serve(args.port)


if __name__ == "__main__":
    main()
