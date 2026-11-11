import * as THREE from "three";
import { STLLoader } from "./vendor/three/STLLoader.js";
import { OrbitControls } from "./vendor/three/OrbitControls.js";

// ordered bed-up: the order you'd stack them
const PARTS = [
  { id: "module_bottom", name: "Bottom", size: "50 x 50", pitch: 90,
    desc: "Holds the ESP32 upright with USB down, plus a buzzer pocket, a cable slot in the back wall and a zip-tie anchor." },
  { id: "module_btn1", name: "One button", size: "50 x 50", pitch: 36,
    desc: "A single Ø19 button on the front face, centred 14.5 mm above the seam." },
  { id: "module_btn2", name: "Two buttons", size: "50 x 50", pitch: 64,
    desc: "One button on the front at 14.5 mm, one on the back at 42.5 mm so the bodies don't collide inside." },
  { id: "module_btn2_diag", name: "Two buttons, diagonal", size: "50 x 50", pitch: 58,
    desc: "Both on the front: lower right and upper left, 7.5 mm off centre." },
  { id: "module_btn2_diag_rev", name: "Two buttons, mirrored", size: "50 x 50", pitch: 58,
    desc: "The diagonal module flipped: lower left and upper right." },
  { id: "module_btn3", name: "Three buttons", size: "50 x 50", pitch: 58,
    desc: "Front lower right, front upper left, and one on the back behind the lower button. Front and back need 19 mm between centres, which is why x stays at ±9.5." },
  { id: "module_btn3_top", name: "Three buttons, tall", size: "50 x 50", pitch: 76,
    desc: "Front diagonal pair plus a back button centred at 50 mm. Taller, so the back button can sit in the middle." },
  { id: "module_top", name: "KY-023 top", size: "50 x 50", pitch: 30,
    desc: "Bench-test ring for the cheap KY-023 stick. Takes the screw-on lid below." },
  { id: "lid_ky023", name: "KY-023 lid", size: "50 x 50", pitch: 3, flip: true,
    desc: "Screw-on plate with a Ø24 opening, two LED holes and two standoffs for the joystick board." },
  { id: "module_top_smc30", name: "Joystick top", size: "50 x 50", pitch: 50, flip: true,
    desc: "Fixed plate for the SMC30B joystick with a Ø32 cutout, four M3 holes and two 3 mm LEDs in the front corners." },
  { id: "module_top_smc30_btn", name: "Joystick top with button", size: "50 x 50", pitch: 71.5, flip: true,
    desc: "Same top, taller, with one button on the front below the joystick body. The body fills the interior, so the button can't sit beside it." },
];

const STACKS = [
  { id: "btn3", title: "Three buttons", height: 198, parts: "Bottom, three buttons, joystick top" },
  { id: "btn3_top", title: "Three buttons, tall", height: 216, parts: "Bottom, tall three-button, joystick top" },
  { id: "top_btn", title: "Button under the stick", height: 219.5, parts: "Bottom, diagonal pair, joystick top with button" },
  { id: "example", title: "Four buttons", height: 234, parts: "Bottom, one button, three buttons, joystick top" },
  { id: "diag", title: "Diagonal", height: 234, parts: "Bottom, one button, diagonal pair, joystick top" },
  { id: "diag_rev", title: "Diagonal, mirrored", height: 234, parts: "Bottom, mirrored pair, one button, joystick top" },
  { id: "ky023", title: "KY-023 bench", height: 181, parts: "Bottom, three buttons, KY-023 top and lid" },
];

const reduced = matchMedia("(prefers-reduced-motion: reduce)").matches;

function setProgress(bed, p) {
  bed.style.setProperty("--p", p.toFixed(4));
  bed.classList.toggle("printing", p > 0.002 && p < 0.998);
}

/* ---------- build DOM ---------- */

const tpl = document.getElementById("part-tpl");
const partsEl = document.getElementById("parts");
for (const part of PARTS) {
  const node = tpl.content.cloneNode(true);
  const art = node.querySelector(".part");
  art.id = part.id;
  art.querySelector("h2").innerHTML = `${part.name}<span class="file">${part.id}.stl</span>`;
  art.querySelector(".dims").innerHTML = `
    <div><dt>Footprint</dt><dd>${part.size}<small>mm</small></dd></div>
    <div><dt>${part.id === "lid_ky023" ? "Plate" : "Adds to stack"}</dt><dd>${part.pitch}<small>mm</small></dd></div>`;
  art.querySelector(".desc").textContent = part.desc;
  const img = art.querySelector("img");
  img.src = `assets/parts/preview_${part.id}.svg`;
  img.alt = `Line drawing of ${part.name}`;
  img.loading = "lazy";
  const bed = art.querySelector(".bed");
  bed.dataset.model = `assets/parts/${part.id}.stl`;
  if (part.flip) bed.dataset.flip = "";
  partsEl.append(node);
}

const stacksEl = document.getElementById("stacks");
for (const s of STACKS) {
  const card = document.createElement("article");
  card.className = "stack-card";
  card.innerHTML = `
    <div class="bed" data-print="view" data-model="assets/stacks/${s.id}_shell.stl" data-inside="assets/stacks/${s.id}_inside.stl" data-esp="assets/stacks/${s.id}_esp.stl">
      <img src="assets/parts/preview_stack_${s.id}.svg" alt="${s.title} stack" width="500" height="700" loading="lazy">
      <span class="nozzle" aria-hidden="true"></span>
      <button class="rotate" type="button" aria-pressed="false">Rotate</button>
    </div>
    <h3>${s.title}</h3>
    <p>${s.height} mm tall. ${s.parts}.</p>`;
  stacksEl.append(card);
}

/* ---------- printing ---------- */

// timed print from the current progress to done
function printOut(bed, ms) {
  cancelAnimationFrame(bed._raf);
  bed._done = true;
  if (reduced) return setProgress(bed, 1);
  const from = parseFloat(bed.style.getPropertyValue("--p")) || 0;
  const t0 = performance.now();
  const step = (t) => {
    const k = Math.min(1, (t - t0) / (ms * (1 - from)));
    setProgress(bed, from + (1 - from) * (1 - Math.pow(1 - k, 2)));
    if (k < 1) bed._raf = requestAnimationFrame(step);
  };
  bed._raf = requestAnimationFrame(step);
}

printOut(document.querySelector('[data-print="load"]'), 2600);

const io = new IntersectionObserver((entries) => {
  let delay = 0;
  for (const e of entries) {
    if (!e.isIntersecting) continue;
    io.unobserve(e.target);
    setTimeout(() => printOut(e.target, 1600), delay);
    delay += 180;
  }
}, { threshold: 0.35 });
document.querySelectorAll('[data-print="view"]').forEach((b) => io.observe(b));

// parts follow the scroll while they come in, then finish on their own once the text is fully readable
const scrollBeds = [...document.querySelectorAll('[data-print="scroll"]')];
function onScroll() {
  const vh = innerHeight;
  for (const bed of scrollBeds) {
    const art = bed.closest(".part");
    const r = art.getBoundingClientRect();
    if (r.bottom < 0 || r.top > vh) {
      // off screen: rewind so it prints again next time
      if (bed._done && !bed.classList.contains("is-3d")) {
        cancelAnimationFrame(bed._raf);
        bed._done = false;
        setProgress(bed, 0);
      }
      continue;
    }
    if (bed._done) continue;
    const t = art.querySelector(".part-text").getBoundingClientRect();
    if (reduced || (t.top >= 0 && t.bottom <= vh)) {
      printOut(bed, 1200);
      continue;
    }
    setProgress(bed, Math.min(0.5, Math.max(0, (vh - r.top) / vh) * 0.5));
  }
}
addEventListener("scroll", onScroll, { passive: true });
addEventListener("resize", onScroll);
onScroll();

/* ---------- 3D viewer ---------- */

const loader = new STLLoader();
const cache = new Map();
function loadGeometry(url) {
  if (!cache.has(url)) {
    cache.set(url, new Promise((res, rej) => loader.load(url, res, undefined, rej)));
  }
  return cache.get(url);
}

const petg = new THREE.MeshStandardMaterial({ color: 0x2f7f8c, roughness: 0.38, metalness: 0.05, transparent: true, opacity: 0.88 });
const petgGlass = new THREE.MeshStandardMaterial({ color: 0x2f7f8c, roughness: 0.3, transparent: true, opacity: 0.5, depthWrite: false, side: THREE.DoubleSide });
const ghost =new THREE.MeshStandardMaterial({ color: 0xc2412d, roughness: 0.6 });
const board = new THREE.MeshStandardMaterial({ color: 0x3d8b4f, roughness: 0.6 });

// the CadQuery previews look along (0.8, -1.2, 0.7) with Z up; start the camera there
const VIEW = new THREE.Vector3(0.8, -1.2, 0.7).normalize();

async function open3D(bed) {
  const status = document.createElement("span");
  status.className = "status";
  status.textContent = "Loading model…";
  bed.append(status);

  let shell, inside, esp;
  try {
    [shell, inside, esp] = await Promise.all([
      loadGeometry(bed.dataset.model),
      bed.dataset.inside ? loadGeometry(bed.dataset.inside) : null,
      bed.dataset.esp ? loadGeometry(bed.dataset.esp) : null,
    ]);
  } catch {
    status.textContent = location.protocol === "file:"
      ? "3D needs a local server: run python enclosure-site/build.py --serve"
      : "Couldn't load the model file.";
    return null;
  }
  status.remove();

  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
  renderer.setPixelRatio(Math.min(devicePixelRatio, 2));
  bed.prepend(renderer.domElement);

  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(30, 1, 1, 5000);
  camera.up.set(0, 0, 1);
  scene.add(new THREE.HemisphereLight(0xfff6e6, 0x6b5530, 1.6));
  const sun = new THREE.DirectionalLight(0xffffff, 1.8);
  scene.add(sun);

  const group = new THREE.Group();
  // stacks get a thinner shell so the button bodies and ESP32 show through
  group.add(new THREE.Mesh(shell, inside ? petgGlass : petg));
  if (inside) group.add(new THREE.Mesh(inside, ghost));
  if (esp) group.add(new THREE.Mesh(esp, board));
  if ("flip" in bed.dataset) group.rotation.x = Math.PI;
  scene.add(group);

  const box = new THREE.Box3().setFromObject(group);
  const center = box.getCenter(new THREE.Vector3());
  const radius = box.getBoundingSphere(new THREE.Sphere()).radius;
  // fit the silhouette a Z-spinning part sweeps, not the bounding sphere, so tall stacks fill the frame
  const size = box.getSize(new THREE.Vector3());
  const spin = Math.hypot(size.x, size.y) / 2;
  const elev = Math.asin(VIEW.z);
  const halfH = (size.z / 2) * Math.cos(elev) + spin * Math.sin(elev);
  const aspect = bed.clientWidth / bed.clientHeight;
  const tanV = Math.tan(THREE.MathUtils.degToRad(camera.fov / 2));
  const dist = Math.max(halfH / tanV, spin / (tanV * aspect)) * 1.08 + spin;
  camera.position.copy(center).addScaledVector(VIEW, dist);
  sun.position.copy(camera.position).add(new THREE.Vector3(-radius, -radius, radius * 2));

  const controls = new OrbitControls(camera, renderer.domElement);
  controls.target.copy(center);
  controls.enableDamping = true;
  controls.enablePan = false;
  controls.minDistance = radius * 1.2;
  controls.maxDistance = dist * 2.5;
  controls.autoRotate = !reduced;
  controls.autoRotateSpeed = 1.6;
  controls.addEventListener("start", () => (controls.autoRotate = false));

  let raf = 0;
  const resize = () => {
    const { clientWidth: w, clientHeight: h } = bed;
    renderer.setSize(w, h, false);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  };
  const ro = new ResizeObserver(resize);
  ro.observe(bed);
  resize();
  const tick = () => {
    controls.update();
    renderer.render(scene, camera);
    raf = requestAnimationFrame(tick);
  };
  tick();

  return () => {
    cancelAnimationFrame(raf);
    ro.disconnect();
    controls.dispose();
    renderer.dispose();
    renderer.domElement.remove();
  };
}

for (const btn of document.querySelectorAll(".rotate")) {
  let close = null;
  btn.addEventListener("click", async () => {
    const bed = btn.closest(".bed");
    if (close) {
      close();
      close = null;
      bed.classList.remove("is-3d");
      btn.setAttribute("aria-pressed", "false");
      btn.textContent = "Rotate";
      return;
    }
    btn.disabled = true;
    close = await open3D(bed);
    btn.disabled = false;
    if (!close) return;
    cancelAnimationFrame(bed._raf);
    bed._done = true;
    setProgress(bed, 1);
    bed.classList.add("is-3d");
    btn.setAttribute("aria-pressed", "true");
    btn.textContent = "Drawing";
  });
}
