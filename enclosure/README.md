# Modular remote enclosure
A stackable 50 x 50 mm enclosure for testing button layouts. Every module has the same footprint and joint, so you can stack them in any order and turn each one in 90° steps to move its buttons to another face.

![stack with 3-button module](out/preview_stack_btn3.svg)

## Files
`enclosure.py` holds every dimension as a named constant at the top. Edit a value and regenerate:

```
python enclosure/enclosure.py
```

`out/` contains:

| File | Use |
|---|---|
| `*.step` | Open or edit in Fusion (assembly coordinates) |
| `*.stl` | Bambu Studio, already placed in print orientation |
| `assembly_*.step` | Example stacks: ESP32 envelope in green, button bodies and joystick in red |
| `preview_*.svg`, `cutaway_*.svg` | Quick look without CAD |

The script also checks every example stack for overlaps between parts, button bodies (Ø19 x 39 deep, nut Ø22), the SMC30B body and the ESP32 board envelope. A "graze" under 0.05 mm³ is the round nut envelope touching a corner fillet: the real hex nut clears it, at worst turn it a few degrees.

## Parts
| Part | Outer size (mm) | Contents |
|---|---|---|
| `module_top_smc30` | 50 x 50, pitch 50 | Top with fixed plate for the SMC30B joystick, 2 LEDs (3 mm) at the front edge |
| `module_top` + `lid_ky023` | pitch 30 + 3 plate | Bench-test top for a KY-023: ring plus screw-on lid with Ø24 opening, 2 LEDs, 2 standoffs |
| `module_btn1` | 50 x 50, pitch 36 | 1 button, front face, centre 14.5 above the seam |
| `module_btn2` | 50 x 50, pitch 64 | 2 buttons, front at 14.5 and back at 42.5 |
| `module_btn3` | 50 x 50, pitch 58 | 3 buttons: front lower-right (x +9.5, z 12), front upper-left (x -9.5, z 32), back upper-right (x +9.5, z 32), x seen from the front |
| `module_bottom` | 50 x 50 x 90 | ESP32 holder, buzzer pocket, cable slot, zip-tie anchor |

"Pitch" is the height a module adds to the stack. Example stacks with 3 buttons each: bottom + btn3 + SMC30B top = 198 mm, bottom + btn1 + btn2 + SMC30B top = 240 mm.

## Key dimensions
| Item | Value | Source |
|---|---|---|
| Wall | 3.0 | |
| Interior | 44 x 44, inner corner radius 2 | |
| Button hole | Ø19.4 | Funduino drawing: thread 19, head Ø21.8, nut 22 across corners, max panel 10 |
| Button depth behind wall | ~31 + wires | drawing: 33 overall |
| LED hole | Ø3.2, counterbore Ø4.2 x 1.2 from inside | 3 mm LED flange |
| Buzzer pocket | Ø12.3, 1 x Ø2.5 + 4 x Ø1.5 sound holes | buzzer Ø12 x 8.2 |
| ESP32 slot | PCB 55 x 28 x 1.6, 5 mm behind for components | **measure your board** (`ESP_LEN`, `ESP_W`) |
| Floor to PCB edge | 16 | for a 90° USB-C plug (`USB_CLEAR`) |
| Cable slot | 4.4 wide x 13 high, back wall, open at the bottom | |
| Joint overlap | 7, clearance 0.2 per side | |
| SMC30B cutout | Ø32, 4 x Ø3.4 on 28 x 28 | XL catalogue p.5: flange 38 x 38 x 6 under the plate, body Ø34.9 x 33, boot base Ø31 |
| SMC30B top plate | 3 thick, flange underside 47 above the seam | |
| KY-023 standoffs | 17 long, Ø6, Ø2.6 hole | bench-test lid only |

## Hardware
| Item | Qty |
|---|---|
| M3 heat-set insert, 5.7 long (hole Ø4.0) | 4 per module, lid excluded |
| M3 x 6 button head screw (ISO 7380) | 4 per joint, lid included |
| M3 x 10 screw + nut (SMC30B: 3 plate + 6 flange) | 4 |
| Small zip tie (≤3.6 wide) | 1 |
| Double-sided foam tape or hot glue | ESP32 |

## Print settings (X1C, PETG)
- 0.2 mm layers, 4 walls, 15% gyroid, no supports.
- `module_top_smc30` and `lid_ky023` print face down. Everything else prints as assembled (skirt on the bed).
- Bottom module is 90 mm tall: a brim helps if your PETG bed adhesion is weak.
- The Ø19.4 button holes print horizontally. If a button won't go in, run a 19 mm drill or a file through by hand.
- Print one `module_btn1` and the `module_bottom` first to check the joint fit before printing the rest.

## Assembly
1. Press the heat-set inserts horizontally into the insert bosses near the top of each module (from outside, through the thin band under the joint).
2. Bottom module: slide the ESP32 down between the ribs and the front lips until it sits on the shelf, with the pin side facing forward and USB down. Fix it with foam tape on the ribs or a dab of hot glue.
3. Plug in the USB-C cable and lay it into the slot from below. Zip-tie it to the anchor.
4. Press the buzzer into its pocket, face first.
5. Fit the buttons: thread from outside, nut from inside. Turn each module before stacking to put its buttons on the face you want.
6. Stack the modules, then drive the 4 screws from outside through the skirt into the inserts.
7. SMC30B top: press in the LEDs from below, push the joystick up from below until the boot comes through, and fix it with 4 M3 screws from the top, nuts under the flange. Do this before stacking, since you reach the nuts through the open bottom. The connector bump fits facing any side.

## Open items
- SMC30B: the catalogue says cutout Ø32 and the boot base is Ø31. A 28 mm hole won't let the boot through. Check whether the M3 nuts fit between the flange corners and the Ø34.9 body. If they don't, use screws from below into the plate.
- Armrest mount: glue the bottom on for now. A screw-on adapter can use the flat floor later.
- `ESP_LEN` / `ESP_W`: measure the actual board.
