"""Modular M25 remote enclosure (parametric, CadQuery).

Run:  python enclosure/enclosure.py
Writes STEP + STL per part, an assembly STEP and SVG previews to enclosure/out/.

Axes: X = left/right, Y = front(-)/back(+), Z = up. All units mm.
Every module shares the same 50 x 50 footprint and joint, so modules stack in
any order and can be turned in 90 degree steps.
"""

import math
from pathlib import Path

import cadquery as cq

# ---------------------------------------------------------------- footprint
W = 50.0            # outer width and depth
R = 6.0             # outer corner radius
T = 3.0             # wall thickness
IN = W - 2 * T      # interior width (44)
R_IN = 2.0          # tighter than R - T so off-centre button nuts clear the corner

# ---------------------------------------------------------------- joint
L = 7.0             # overlap of the upper skirt over the lower module
TS = 1.4            # skirt thickness (outer half of the wall)
C = 0.2             # sliding clearance per side (PETG on X1C)
TOP_GAP = 0.2       # lower module stops short so the skirt shoulder carries the load
FUNNEL = 1.6        # 45 deg chamfer above the skirt, keeps the part support-free

# Horizontal M3 screws, one per face, pinwheel pattern so 90 deg turns line up.
SCREW_OFF = 16.0    # offset along the face from its centre
SCREW_Z = L / 2     # below the joint line
INSERT_D = 4.0      # hole for M3 x 5.7 heat-set insert
INSERT_DEPTH = 6.0
SCREW_CLEAR_D = 3.4
BOSS_W = 9.0
BOSS_DEPTH = 6.0    # how far the insert boss reaches into the cavity
BOSS_H = 8.3

# ---------------------------------------------------------------- button (Funduino 19 mm LED push button)
BTN_HOLE_D = 19.4   # thread 19 mm + clearance
BTN_HEAD_D = 21.8
BTN_PITCH = 28.0    # vertical spacing in the 2-button module
BTN_Z0 = 14.5       # first button centre above module bottom
BTN_BODY_D = 19.0   # thread, the widest part behind the wall
BTN_BODY_LEN = 39.0 # 31 body + bent wires
BTN_NUT_D = 22.0
BTN_NUT_T = 4.0

# 3-button module: two on the front on a diagonal, one on the back.
# (x, z, face); x is the global X position, seen from the front.
BTN3_X = 9.5
BTN3 = [(BTN3_X, 12.0, "front"), (-BTN3_X, 32.0, "front"), (BTN3_X, 32.0, "back")]

# ---------------------------------------------------------------- module heights (stack pitch)
H_TOP = 30.0
H_BTN1 = 36.0
H_BTN2 = 64.0
H_BTN3 = 58.0       # upper buttons must stay below the insert bosses of the joint above
H_BOTTOM = 90.0

# ---------------------------------------------------------------- KY-023 lid (bench tests)
LID_T = 3.0
JOY_HOLE_D = 24.0
JOY_STANDOFF_LEN = 17.0      # lid underside to joystick PCB top, PLACEHOLDER
JOY_STANDOFF_D = 6.0
JOY_SCREW_HOLE_D = 2.6       # M3 self-tapping
JOY_STANDOFFS = [(14.5, -10.5), (-14.5, 10.5)]   # diagonal pair, avoids the joint bosses
LED_HOLE_D = 3.2
LED_FLANGE_D = 4.2
LED_FLANGE_DEPTH = 1.2
LEDS = [(-8.0, -18.0), (8.0, -18.0)]

# ---------------------------------------------------------------- SMC30B joystick (XL catalogue p.5)
# Mounts from below: the 38 mm flange sits under the plate, only the boot comes through.
SMC_CUTOUT_D = 32.0          # drawing; boot base is Ø31
SMC_HOLE_PITCH = 28.0        # 4 x Ø3.2 on a 28 x 28 square
SMC_SCREW_D = 3.4
SMC_FLANGE = 38.0
SMC_FLANGE_T = 6.0
SMC_BODY_D = 34.9
SMC_BODY_LEN = 33.0
SMC_TAB_W = 18.0             # connector bump at the bottom of the body
SMC_TAB_R = 19.5
SMC_TAB_H = 8.0              # not dimensioned, estimated from the side view
H_TOP_SMC = 50.0             # plate + flange + body + room for the cable
TOP_PLATE_T = 3.0
SMC_LEDS = [(-8.0, -21.3), (8.0, -21.3)]   # in front of the flange, LED pocket cuts into the wall

# ---------------------------------------------------------------- bottom module
FLOOR_T = 3.0
ESP_LEN = 55.0      # 38-pin DevKit, measure yours (clones 51-55)
ESP_W = 28.0
ESP_PCB_T = 1.6
ESP_GAP = 5.0       # wall to component side of the PCB
USB_CLEAR = 16.0    # floor to PCB edge, enough for a 90 deg USB-C plug
CABLE_SLOT_W = 4.4
CABLE_SLOT_H = 13.0
BUZZER_D = 12.3
BUZZER_Y = -8.0

OUT = Path(__file__).resolve().parent / "out"


# ================================================================= helpers
def rbox(w, h, r, z0=0.0):
    return cq.Workplane("XY").workplane(offset=z0).rect(w, w).extrude(h).edges("|Z").fillet(r)


def rsketch(w, r):
    return cq.Sketch().rect(w, w).vertices().fillet(r)


def rloft(w0, r0, z0, w1, r1, z1):
    return (
        cq.Workplane("XY")
        .workplane(offset=z0)
        .placeSketch(rsketch(w0, r0), rsketch(w1, r1).moved(cq.Location(cq.Vector(0, 0, z1 - z0))))
        .loft()
    )


def pinwheel(shape):
    """Copy a front-face feature onto all four faces (90 deg steps)."""
    out = shape
    for a in (90, 180, 270):
        out = out.union(shape.rotate((0, 0, 0), (0, 0, 1), a))
    return out


def y_cyl(d, y0, y1, x, z):
    """Cylinder along Y from y0 to y1."""
    return (
        cq.Workplane("XZ", origin=(x, y1, z))
        .circle(d / 2)
        .extrude(y1 - y0)
    )


def x_cyl(d, x0, x1, y, z):
    return cq.Workplane("YZ", origin=(x0, y, z)).circle(d / 2).extrude(x1 - x0)


# ================================================================= joint features
def joint_top(body, H):
    """Recess the outer half of the wall at the top and add insert bosses."""
    recess_w = W - 2 * (TS + C)
    ring = rbox(W + 4, L, R + 2, H - L).cut(rbox(recess_w, L, R - TS - C, H - L))
    body = body.cut(ring)
    body = body.cut(rbox(W + 4, TOP_GAP + 1, R + 2, H - TOP_GAP))
    # 45 deg chamfer on the band top so it clears the upper module's funnel
    zc = H - TOP_GAP - FUNNEL
    keep = rloft(recess_w, R - TS - C, zc, recess_w - 2 * FUNNEL + 0.01, max(R - TS - C - FUNNEL, 0.5), H - TOP_GAP)
    body = body.cut(rbox(W + 4, FUNNEL, R + 2, zc).cut(keep))

    # insert boss on the front face, then pinwheel it
    bx0 = SCREW_OFF - BOSS_W / 2
    zb = H - TOP_GAP - BOSS_H
    yi = -IN / 2
    boss = cq.Workplane("XY").box(BOSS_W, BOSS_DEPTH, BOSS_H, centered=False).translate((bx0, yi, zb))
    gusset = (
        cq.Workplane("YZ", origin=(bx0, 0, 0))
        .polyline([(yi, zb), (yi + BOSS_DEPTH, zb), (yi, zb - BOSS_DEPTH)])
        .close()
        .extrude(BOSS_W)
    )
    body = body.union(pinwheel(boss.union(gusset)))
    hole = y_cyl(INSERT_D, -W / 2 - 1, -(W / 2 - TS - C) + INSERT_DEPTH, SCREW_OFF, H - SCREW_Z)
    return body.cut(pinwheel(hole))


def joint_bottom(body):
    """Skirt over the module below, with the 45 deg funnel and screw holes."""
    skirt_in = W - 2 * TS
    skirt = rbox(W, L, R, -L).cut(rbox(skirt_in, L, R - TS, -L))
    fill = rbox(skirt_in, FUNNEL, R - TS, -FUNNEL).cut(
        rloft(skirt_in, R - TS, -FUNNEL, IN, R_IN, 0.0)
    )
    body = body.union(skirt).union(fill)
    hole = y_cyl(SCREW_CLEAR_D, -W / 2 - 1, -W / 2 + TS + 1, SCREW_OFF, -SCREW_Z)
    return body.cut(pinwheel(hole))


def shell(H, skirt=True, floor=False):
    body = rbox(W, H, R)
    if skirt:
        body = joint_bottom(body)
    z_cav = FLOOR_T if floor else -L - 1
    body = body.cut(rbox(IN, H - z_cav + 2, R_IN, z_cav))
    if floor:
        body = body.faces("<Z").chamfer(0.5)
    return joint_top(body, H)


def on_face(make, x, face):
    """Build a feature on the front face, or turn it 180 deg onto the back.

    x is always the global X position, so front and back features line up as seen from the front.
    """
    if face == "front":
        return make(x)
    return make(-x).rotate((0, 0, 0), (0, 0, 1), 180)


def button_hole(z, face="front", x=0.0):
    return on_face(lambda xl: y_cyl(BTN_HOLE_D, -W / 2 - 1, -IN / 2 + 1, xl, z), x, face)


def button_dummy(z, face="front", x=0.0):
    """Body + nut envelope behind the wall, used only for the clearance check."""
    yi = -IN / 2

    def make(xl):
        body = y_cyl(BTN_BODY_D, yi, yi + BTN_BODY_LEN, xl, z)
        return body.union(y_cyl(BTN_NUT_D, yi + 0.01, yi + BTN_NUT_T, xl, z))

    return on_face(make, x, face)


# ================================================================= parts
def module_top():
    return shell(H_TOP)


def module_btn1():
    return shell(H_BTN1).cut(button_hole(BTN_Z0, "front"))


def module_btn2():
    return (
        shell(H_BTN2)
        .cut(button_hole(BTN_Z0, "front"))
        .cut(button_hole(BTN_Z0 + BTN_PITCH, "back"))
    )


def module_btn3():
    body = shell(H_BTN3)
    for x, z, face in BTN3:
        body = body.cut(button_hole(z, face, x))
    return body


def lid():
    """Assembly coordinates: plate z 0..LID_T, skirt below. Print upside down."""
    plate = rbox(W, LID_T, R).faces(">Z").edges().fillet(1.2)
    skirt = rbox(W, L, R, -L).cut(rbox(W - 2 * TS, L, R - TS, -L))
    body = plate.union(skirt)
    body = body.cut(pinwheel(y_cyl(SCREW_CLEAR_D, -W / 2 - 1, -W / 2 + TS + 1, SCREW_OFF, -SCREW_Z)))
    body = body.cut(cq.Workplane("XY").circle(JOY_HOLE_D / 2).extrude(LID_T * 3).translate((0, 0, -LID_T)))
    for x, y in LEDS:
        body = body.cut(cq.Workplane("XY").center(x, y).circle(LED_HOLE_D / 2).extrude(LID_T * 3).translate((0, 0, -LID_T)))
        body = body.cut(cq.Workplane("XY").center(x, y).circle(LED_FLANGE_D / 2).extrude(LED_FLANGE_DEPTH))
    for x, y in JOY_STANDOFFS:
        post = (
            cq.Workplane("XY").center(x, y).circle(JOY_STANDOFF_D / 2).extrude(-JOY_STANDOFF_LEN)
            .faces("<Z").workplane().hole(JOY_SCREW_HOLE_D, 10)
        )
        body = body.union(post)
    return body


def module_top_smc30():
    """Top with integral plate for the SMC30B. Assembly coordinates; prints upside down."""
    H = H_TOP_SMC
    body = rbox(W, H, R)
    body = joint_bottom(body)
    body = body.cut(rbox(IN, H - TOP_PLATE_T + L + 1, R_IN, -L - 1))
    body = body.faces(">Z").edges().fillet(1.2)
    zp = H - TOP_PLATE_T
    body = body.cut(cq.Workplane("XY").circle(SMC_CUTOUT_D / 2).extrude(TOP_PLATE_T + 2).translate((0, 0, zp - 1)))
    h = SMC_HOLE_PITCH / 2
    for x, y in [(h, h), (-h, h), (h, -h), (-h, -h)]:
        body = body.cut(cq.Workplane("XY").center(x, y).circle(SMC_SCREW_D / 2).extrude(TOP_PLATE_T + 2).translate((0, 0, zp - 1)))
    for x, y in SMC_LEDS:
        body = body.cut(cq.Workplane("XY").center(x, y).circle(LED_HOLE_D / 2).extrude(TOP_PLATE_T + 2).translate((0, 0, zp - 1)))
        body = body.cut(cq.Workplane("XY").center(x, y).circle(LED_FLANGE_D / 2).extrude(LED_FLANGE_DEPTH + 6).translate((0, 0, zp - 6)))
    return body


def smc30_dummy():
    """Joystick envelope below the plate, used only for the clearance check. Connector faces back."""
    zp = H_TOP_SMC - TOP_PLATE_T
    flange = cq.Workplane("XY").box(SMC_FLANGE, SMC_FLANGE, SMC_FLANGE_T).translate((0, 0, zp - SMC_FLANGE_T / 2))
    z_body = zp - SMC_FLANGE_T - SMC_BODY_LEN
    body = cq.Workplane("XY").circle(SMC_BODY_D / 2).extrude(SMC_BODY_LEN).translate((0, 0, z_body))
    tab = cq.Workplane("XY").box(SMC_TAB_W, SMC_TAB_R, SMC_TAB_H, centered=(True, False, False)).translate((0, 0, z_body))
    return flange.union(body).union(tab)


def module_bottom():
    body = shell(H_BOTTOM, skirt=False, floor=True)
    z0 = FLOOR_T + USB_CLEAR            # PCB bottom (USB) edge
    y_back = IN / 2 - ESP_GAP           # PCB component side
    y_front = y_back - ESP_PCB_T        # PCB pin side
    yw = IN / 2

    # standoff ribs behind the PCB, outside the module can and the header solder tips
    for s in (-1, 1):
        rib = cq.Workplane("XY").box(2.6, ESP_GAP + 0.5, ESP_LEN - 4, centered=False)
        body = body.union(rib.translate((s * 10.5 - 1.3, y_back, z0 + 3)))

    # shelf under the USB edge, split around the plug; front lips keep the PCB in
    lip_y0 = y_front - C - 1.5
    for s in (-1, 1):
        x0 = 7.0 if s > 0 else -13.0
        shelf = cq.Workplane("XY").box(6, yw - lip_y0 + 0.5, 3, centered=False).translate((x0, lip_y0, z0 - 3))
        gusset = (
            cq.Workplane("YZ", origin=(x0, 0, 0))
            .polyline([(lip_y0, z0 - 3), (yw + 0.5, z0 - 3), (yw + 0.5, z0 - 3 - (yw - lip_y0))])
            .close()
            .extrude(6)
        )
        lx0 = 7.0 if s > 0 else -11.0
        lip = cq.Workplane("XY").box(4, 1.5, 1.5, centered=False).translate((lx0, lip_y0, z0))
        body = body.union(shelf).union(gusset).union(lip)

    # cable slot, open to the bottom edge of the back wall
    slot = cq.Workplane("XY").box(CABLE_SLOT_W, T + 2, CABLE_SLOT_H - CABLE_SLOT_W / 2 + 1, centered=False)
    slot = slot.translate((-CABLE_SLOT_W / 2, yw - 0.5, -1))
    slot = slot.union(y_cyl(CABLE_SLOT_W, yw - 0.5, W / 2 + 1, 0, CABLE_SLOT_H - CABLE_SLOT_W / 2))
    body = body.cut(slot)

    # zip-tie anchor for strain relief
    anchor = cq.Workplane("XY").box(8, 6, 5, centered=False).translate((-14, -3, FLOOR_T))
    tunnel = cq.Workplane("XY").box(10, 4, 2, centered=False).translate((-15, -2, FLOOR_T))
    body = body.union(anchor).cut(tunnel)

    # buzzer pocket on the left wall, sound holes through it
    bz = FLOOR_T + 7.5
    xw = -IN / 2
    tube = x_cyl(15.0, xw - 0.5, xw + 6.5, BUZZER_Y, bz).cut(x_cyl(BUZZER_D, xw - 1, xw + 7, BUZZER_Y, bz))
    body = body.union(tube)
    body = body.cut(x_cyl(2.5, -W / 2 - 1, xw + 0.5, BUZZER_Y, bz))
    for a in range(0, 360, 90):
        dy = 3.5 * math.cos(math.radians(a + 45))
        dz = 3.5 * math.sin(math.radians(a + 45))
        body = body.cut(x_cyl(1.5, -W / 2 - 1, xw + 0.5, BUZZER_Y + dy, bz + dz))
    return body


# ================================================================= build
def esp_dummy():
    """Board envelope used only for the clearance check."""
    z0 = FLOOR_T + USB_CLEAR
    y_back = IN / 2 - ESP_GAP
    pcb = cq.Workplane("XY").box(ESP_W, ESP_PCB_T, ESP_LEN, centered=False)
    return pcb.translate((-ESP_W / 2, y_back - ESP_PCB_T, z0))


def main():
    OUT.mkdir(exist_ok=True)
    parts = {
        "lid_ky023": lid(),
        "module_top": module_top(),
        "module_top_smc30": module_top_smc30(),
        "module_btn1": module_btn1(),
        "module_btn2": module_btn2(),
        "module_btn3": module_btn3(),
        "module_bottom": module_bottom(),
    }
    buttons = {
        "module_btn1": [(0.0, BTN_Z0, "front")],
        "module_btn2": [(0.0, BTN_Z0, "front"), (0.0, BTN_Z0 + BTN_PITCH, "back")],
        "module_btn3": BTN3,
    }

    # print orientation: lid and SMC30 top face down, everything else as assembled
    flipped = ("lid_ky023", "module_top_smc30")
    for name, part in parts.items():
        printable = part.rotate((0, 0, 0), (1, 0, 0), 180) if name in flipped else part
        bb = printable.val().BoundingBox()
        printable = printable.translate((0, 0, -bb.zmin))
        cq.exporters.export(part, str(OUT / f"{name}.step"))
        cq.exporters.export(printable, str(OUT / f"{name}.stl"), tolerance=0.02, angularTolerance=0.1)
        bb = printable.val().BoundingBox()
        print(f"{name:17s} {bb.xlen:6.1f} x {bb.ylen:6.1f} x {bb.zlen:6.1f} mm  vol {printable.val().Volume()/1000:6.1f} cm3")

    def overlaps(shapes):
        # below 0.05 mm3 is the round nut envelope grazing a corner fillet; the hex nut clears it
        found = False
        for i in range(len(shapes)):
            for j in range(i + 1, len(shapes)):
                v = shapes[i][1].intersect(shapes[j][1]).val().Volume()
                if v > 0.05:
                    found = True
                    print(f"  OVERLAP {shapes[i][0]} / {shapes[j][0]}: {v:.2f} mm3")
                elif v > 0.001:
                    print(f"  graze   {shapes[i][0]} / {shapes[j][0]}: {v:.3f} mm3")
        return found

    bad = False
    heights = {"module_bottom": H_BOTTOM, "module_btn1": H_BTN1, "module_btn2": H_BTN2,
               "module_btn3": H_BTN3, "module_top": H_TOP, "module_top_smc30": H_TOP_SMC}
    stacks = {
        "example": ["module_bottom", "module_btn1", "module_btn2", "module_top_smc30"],
        "btn3": ["module_bottom", "module_btn3", "module_top_smc30"],
        "ky023": ["module_bottom", "module_btn3", "module_top"],
    }
    stack_shapes = {}
    for sname, stack in stacks.items():
        asm = cq.Assembly()
        placed = []
        z = 0.0
        for name in stack:
            p = parts[name].translate((0, 0, z))
            asm.add(p, name=name)
            placed.append((name, p))
            for k, (bx, bz, face) in enumerate(buttons.get(name, [])):
                d = button_dummy(bz, face, bx).translate((0, 0, z))
                asm.add(d, name=f"{name}_button{k}", color=cq.Color("red"))
                placed.append((f"{name}_button{k}", d))
            if name == "module_top_smc30":
                d = smc30_dummy().translate((0, 0, z))
                asm.add(d, name="smc30_envelope", color=cq.Color("red"))
                placed.append(("smc30_envelope", d))
            z += heights[name]
        if stack[-1] == "module_top":
            p = parts["lid_ky023"].translate((0, 0, z))
            asm.add(p, name="lid_ky023")
            placed.append(("lid_ky023", p))
            z += LID_T
        asm.add(esp_dummy(), name="esp32_envelope", color=cq.Color("green"))
        asm.export(str(OUT / f"assembly_{sname}.step"))
        print(f"stack {sname}: {z:.1f} mm")
        # every part, button body and the ESP32 against each other
        bad |= overlaps(placed + [("esp32", esp_dummy())])
        stack_shapes[sname] = [p for n, p in placed if "_button" not in n and "envelope" not in n]
    print("clearance check:", "FAIL" if bad else "ok")

    view = (0.8, -1.2, 0.7)
    opts = {"width": 500, "height": 700, "showAxes": False, "projectionDir": view, "showHidden": False}

    def svg(shape, path, **o):
        # the SVG exporter draws Z pointing down; spin about the view axis to fix it
        cq.exporters.export(shape.rotate((0, 0, 0), view, 180), str(path), opt={**opts, **o})

    for sname, shapes in stack_shapes.items():
        combined = shapes[0]
        for p in shapes[1:]:
            combined = combined.union(p)
        svg(combined, OUT / f"preview_stack_{sname}.svg")
    # cutaways remove the front-right quarter (the side facing the camera)
    cutter = cq.Workplane("XY").box(W, W, 400, centered=(False, False, True))
    for name, part in parts.items():
        svg(part, OUT / f"preview_{name}.svg", width=400, height=400)
        svg(part.cut(cutter.translate((0, -W, 0))), OUT / f"cutaway_{name}.svg", width=400, height=400)


if __name__ == "__main__":
    main()
