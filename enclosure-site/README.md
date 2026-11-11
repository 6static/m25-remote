# Enclosure preview site
A one-page viewer for the parts in `../enclosure`. Each part prints layer by layer as it comes into view, and **Rotate** swaps the line drawing for a 3D model you can drag around.

## Run
Needs the CadQuery interpreter (`pip install -r enclosure-site/requirements.txt`).

```
python enclosure-site/build.py --serve           # build assets, serve on :8765, open the browser
python enclosure-site/build.py --regen --serve   # run enclosure/enclosure.py first
python enclosure-site/build.py                   # build only
```

`assets/` is gitignored and built from `enclosure/out`: the previews get a fitted viewBox (CadQuery's SVGs have none), the part STLs are copied, and each assembly STEP is split into three STLs (printed shell, button and joystick bodies, ESP32 envelope). Only files older than their source are rebuilt.

The 3D models load with `fetch`, so opening `index.html` straight from disk shows the drawings but not the 3D view.

## GitHub Pages
`.github/workflows/enclosure-site.yml` builds and deploys the site on every push to `main` that touches `enclosure/` or `enclosure-site/`. Run it by hand with **regen** ticked to rebuild the enclosure in CI too.

Part names and descriptions live in `app.js` (`PARTS`, `STACKS`).
