"""Procedural tire-mark decal masks for HAL_Future_Creation.

Offline and deterministic (fixed seeds); no Unreal Editor required.
Run:  python Tools/TireMarks/GenerateTireMarks.py

Writes to SourceArt/TireMarks/:
  T_TireMark_<Name>_Mask.png     8-bit grayscale, white = ink. Import with
                                 sRGB off + Compression "Masks"; drive decal opacity.
  T_TireMark_<Name>_Preview.png  RGBA black-on-transparent, for review or direct use.
  TireMark_ContactSheet.png      every variant on a light floor, for side-by-side review.

X is the travel direction; the strip variants tile seamlessly along X.
"""
import pathlib

import numpy as np
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceArt' / 'TireMarks'
SS = 2  # supersample factor, downsampled at the end for anti-aliasing

# Tread cross-section, top edge -> bottom edge of the contact patch.
# weight = share of the tire width; 'gap' = circumferential groove (no ink).
TREAD = [
    ('lug', 0.300, dict(slope=+0.55, period=0.070, slit=0.22)),
    ('gap', 0.026, {}),
    ('rib', 0.074, {}),
    ('gap', 0.022, {}),
    ('rib', 0.074, {}),
    ('gap', 0.022, {}),
    ('rib', 0.074, {}),
    ('gap', 0.026, {}),
    ('lug', 0.300, dict(slope=-0.55, period=0.070, slit=0.22)),
]


def smoothstep(a, b, x):
    t = np.clip((x - a) / np.maximum(np.subtract(b, a), 1e-6), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _grid(w, h, rx, ry, rng):
    """One octave of value noise, periodic along X."""
    rx = max(2, min(rx, w // 2))
    ry = max(2, min(ry, h // 2))
    g = rng.random((ry + 1, rx), dtype=np.float32)
    g = np.concatenate([g, g[:, :1]], axis=1)          # wrap in X
    cell_x = int(np.ceil(w / rx))
    cell_y = int(np.ceil(h / ry))
    img = Image.fromarray((g * 255.0).astype(np.uint8))
    img = img.resize(((rx + 1) * cell_x, (ry + 1) * cell_y), Image.BICUBIC)
    return np.asarray(img, dtype=np.float32)[:h, :w] / 255.0


def fbm(w, h, rng, rx=16, ry=4, octaves=5, gain=0.55):
    total = np.zeros((h, w), np.float32)
    amp, norm = 1.0, 0.0
    for i in range(octaves):
        total += amp * _grid(w, h, rx * 2 ** i, ry * 2 ** i, rng)
        norm += amp
        amp *= gain
    total /= norm
    lo, hi = float(total.min()), float(total.max())
    return (total - lo) / max(hi - lo, 1e-6)


def fbm1d(w, rng, rx=6, octaves=3):
    return fbm(w, 4, rng, rx=rx, ry=2, octaves=octaves)[0]


def blur_x(a, k):
    """Box blur along X with wrap-around, so tiling survives."""
    k = int(k)
    if k <= 1:
        return a
    pad = k // 2
    ap = np.concatenate([a[:, -pad:], a, a[:, :k - pad]], axis=1)
    c = np.cumsum(ap, axis=1, dtype=np.float32)
    c = np.concatenate([np.zeros((a.shape[0], 1), np.float32), c], axis=1)
    return ((c[:, k:] - c[:, :-k]) / k)[:, :a.shape[1]]


def tread(w, h, rng, wobble=0.012, lug_scale=1.0, slit=None):
    """Hard tread footprint: 1 = rubber touching the floor."""
    xs = np.arange(w, dtype=np.float32)[None, :]
    ys = np.arange(h, dtype=np.float32)[:, None]
    yy = ys - (fbm1d(w, rng, rx=5)[None, :] * 2.0 - 1.0) * wobble * h
    v = yy / h

    pat = np.zeros((h, w), np.float32)
    total = sum(b[1] for b in TREAD)
    y0 = 0.0
    for kind, weight, p in TREAD:
        y1 = y0 + weight / total
        band = (v >= y0) & (v < y1)
        if kind == 'rib':
            pat = np.where(band, 1.0, pat)
        elif kind == 'lug':
            per = max(4.0, p['period'] * lug_scale * w)
            u = np.mod(xs + p['slope'] * (yy - y0 * h), per) / per
            cut = p['slit'] if slit is None else slit
            pat = np.where(band, (u > cut).astype(np.float32), pat)
        y0 = y1
    return pat


def wear_field(w, h, rng, streak=0.32, fine=0.18):
    """0..1 field driving where rubber is missing. Chips + horizontal scratches."""
    blob = fbm(w, h, rng, rx=18, ry=7, octaves=6, gain=0.58)
    scr = fbm(w, h, rng, rx=5, ry=40, octaves=4, gain=0.55)     # scratches along travel
    spk = fbm(w, h, rng, rx=70, ry=26, octaves=3, gain=0.50)    # fine speckle
    f = (1.0 - streak - fine) * blob + streak * scr + fine * spk
    lo, hi = float(f.min()), float(f.max())
    return (f - lo) / max(hi - lo, 1e-6)


def grunge(pat, rng, thresh=0.30, soft=0.04, pressure=0.16, streak=0.32, fine=0.18):
    """Break the footprint up with a hard cut, so edges stay crisp and chipped."""
    h, w = pat.shape
    f = wear_field(w, h, rng, streak=streak, fine=fine)
    # Vary coverage along the strip by biasing the threshold, not by fading to grey.
    load = fbm1d(w, rng, rx=4)[None, :]
    t = thresh + pressure * (1.0 - load)
    return np.clip(pat * smoothstep(t, t + soft, f), 0.0, 1.0)


def resharpen(a, lo=0.14, hi=0.42):
    """Pull contrast back after a smear, instead of leaving flat grey."""
    return smoothstep(lo, hi, a)


def fade_ends(a, head=0.10, tail=0.18):
    """Soften the two ends so a one-shot decal does not stop on a hard edge."""
    h, w = a.shape
    t = np.linspace(0.0, 1.0, w, dtype=np.float32)[None, :]
    return a * smoothstep(0.0, max(head, 1e-3), t) * (1.0 - smoothstep(1.0 - max(tail, 1e-3), 1.0, t))


def downsample(a):
    h, w = a.shape
    return a.reshape(h // SS, SS, w // SS, SS).mean(axis=(1, 3))


def save(name, ink):
    OUT.mkdir(parents=True, exist_ok=True)
    m = np.clip(downsample(ink), 0.0, 1.0)
    u8 = (m * 255.0 + 0.5).astype(np.uint8)
    Image.fromarray(u8, 'L').save(OUT / f'T_TireMark_{name}_Mask.png')
    rgba = np.zeros((*u8.shape, 4), np.uint8)
    rgba[..., 3] = u8
    Image.fromarray(rgba, 'RGBA').save(OUT / f'T_TireMark_{name}_Preview.png')
    print(f'  {name:<14} {u8.shape[1]}x{u8.shape[0]}  coverage {m.mean():5.1%}')
    return m


def variant_heavy(w, h, seed):
    """Fresh, fully loaded print: almost solid, a few torn white chips."""
    rng = np.random.default_rng(seed)
    pat = tread(w, h, rng)
    return grunge(pat, rng, thresh=0.17, pressure=0.12, streak=0.26, fine=0.16)


def variant_worn(w, h, seed):
    """Same tire, much less rubber laid down."""
    rng = np.random.default_rng(seed)
    pat = tread(w, h, rng, wobble=0.018)
    return grunge(pat, rng, thresh=0.32, pressure=0.14, streak=0.28, fine=0.20)


def variant_skid(w, h, seed):
    """Tread at the start, smeared into a burnout streak towards the end."""
    rng = np.random.default_rng(seed)
    pat = tread(w, h, rng, wobble=0.022)
    ink = grunge(pat, rng, thresh=0.20, pressure=0.14, streak=0.34, fine=0.16)

    t = np.linspace(0.0, 1.0, w, dtype=np.float32)[None, :]
    levels = [ink,
              resharpen(blur_x(ink, 11 * SS), 0.16, 0.44),
              resharpen(blur_x(ink, 35 * SS), 0.18, 0.46),
              resharpen(blur_x(ink, 95 * SS), 0.22, 0.50)]
    stops = [0.0, 0.34, 0.66, 1.0]
    out = levels[0]
    for i in range(1, len(levels)):
        k = smoothstep(stops[i - 1], stops[i], t)
        out = out * (1.0 - k) + levels[i] * k

    # Rubber runs out: raise the wear threshold instead of fading to grey.
    f = wear_field(w, h, rng, streak=0.46, fine=0.16)
    out *= smoothstep(0.0, 0.06, f - 0.45 * smoothstep(0.40, 1.0, t))
    return np.clip(out, 0.0, 1.0)


def variant_patch(w, h, seed):
    """Short one-shot stamp with softened ends."""
    rng = np.random.default_rng(seed)
    pat = tread(w, h, rng, wobble=0.016)
    ink = grunge(pat, rng, thresh=0.26, pressure=0.16, streak=0.32, fine=0.18)
    return fade_ends(ink, head=0.10, tail=0.22)


def variant_scuff(w, h, seed):
    """No readable tread: a sideways scrub / drift smudge."""
    rng = np.random.default_rng(seed)
    pat = tread(w, h, rng, wobble=0.030, lug_scale=1.9, slit=0.12)
    ink = grunge(pat, rng, thresh=0.22, pressure=0.14, streak=0.44, fine=0.12)
    ink = resharpen(blur_x(ink, 70 * SS), 0.26, 0.62)

    f = wear_field(w, h, rng, streak=0.48, fine=0.12)
    ink *= smoothstep(0.18, 0.25, f)
    ys = np.linspace(0.0, 1.0, h, dtype=np.float32)[:, None]
    ink *= smoothstep(0.0, 0.10, ys) * (1.0 - smoothstep(0.90, 1.0, ys))
    return np.clip(fade_ends(ink, head=0.06, tail=0.12), 0.0, 1.0)


# --------------------------------------------------------------------------
# Cartoon graffiti variant.
#
# Drawn from polygons instead of noise, to match the flat-fill / tapered-spike /
# zigzag-notch language already used by T_SlipperyTrail_Graffiti_Strokes:
# no grey, no fine speckle, chunky hand-drawn silhouettes only.
# --------------------------------------------------------------------------

TOON_RIBS = [(0.355, 0.428), (0.458, 0.531), (0.561, 0.634)]
TOON_LUGS = [(0.000, 0.300, +0.55), (0.700, 1.000, -0.55)]


def _jitter_ring(pts, rng, amp):
    """Re-trace a polygon with kicked corners and kicked edge midpoints."""
    out = []
    n = len(pts)
    for i in range(n):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % n]
        out.append((ax + rng.uniform(-amp, amp), ay + rng.uniform(-amp, amp)))
        out.append(((ax + bx) * 0.5 + rng.uniform(-amp, amp),
                    (ay + by) * 0.5 + rng.uniform(-amp, amp)))
    return out


def _spike(x, y, length, thick, ang, flip=1.0):
    """Tapered sliver with one notch, in the house graffiti-stroke shape."""
    ca, sa = float(np.cos(ang)), float(np.sin(ang))

    def p(u, v):
        v *= flip
        return (x + u * ca - v * sa, y + u * sa + v * ca)

    return [p(0.0, 0.0),
            p(length * 0.42, -thick * 0.50), p(length * 0.53, -thick * 0.16),
            p(length * 0.78, -thick * 0.60), p(length * 1.00, -thick * 0.10),
            p(length * 0.94, thick * 0.30), p(length * 0.58, thick * 0.26),
            p(length * 0.28, thick * 0.12)]


def _chip(cx, cy, r, rng):
    """Angular bite taken out of the ink."""
    n = int(rng.integers(5, 8))
    pts = []
    for i in range(n):
        a = 2.0 * np.pi * i / n + rng.uniform(-0.25, 0.25)
        rr = r * rng.uniform(0.45, 1.25)
        pts.append((cx + rr * np.cos(a), cy + rr * np.sin(a)))
    return pts


def variant_toon_worn(w, h, seed):
    """Worn tread, redrawn as flat cartoon graffiti. Tiles along X."""
    rng = np.random.default_rng(seed)
    img = Image.new('L', (w, h), 0)
    draw = ImageDraw.Draw(img)
    base = (fbm1d(w, rng, rx=4) * 2.0 - 1.0) * 0.022 * h   # hand-drawn path wobble

    def wy(x):
        return float(base[int(np.clip(x, 0, w - 1))])

    def blit(pts, fill):
        """Draw once per wrap copy so the strip still tiles."""
        for dx in (-w, 0, w):
            draw.polygon([(px + dx, py) for px, py in pts], fill=fill)

    jit = 0.013 * h
    period = 0.075 * w
    count = int(round(w / period))
    period = w / count

    # --- tread blocks -----------------------------------------------------
    for y0f, y1f, slope in TOON_LUGS:
        y0, y1 = y0f * h, y1f * h
        for i in range(count):
            if rng.random() < 0.34:          # missing rubber = whole block gone
                continue
            x = i * period
            bw = period * rng.uniform(0.66, 0.80)
            top, bot = y0, y1
            if rng.random() < 0.30:          # chewed block, only partly printed
                cut = rng.uniform(0.28, 0.52) * (y1 - y0)
                top, bot = (y0 + cut, y1) if rng.random() < 0.5 else (y0, y1 - cut)
            sh = slope * (bot - top)
            quad = [(x, top + wy(x)), (x + bw, top + wy(x + bw)),
                    (x + bw + sh, bot + wy(x + bw)), (x + sh, bot + wy(x))]
            blit(_jitter_ring(quad, rng, jit), 255)

    # --- centre ribs, broken into tapered segments ------------------------
    for y0f, y1f in TOON_RIBS:
        y0, y1 = y0f * h, y1f * h
        x = rng.uniform(0.0, 0.12) * w
        while x < w:
            seg = rng.uniform(0.12, 0.30) * w
            x1 = min(x + seg, w * 1.02)
            steps = max(6, int(seg / (0.012 * w)))
            us = np.linspace(x, x1, steps)
            tl = seg * rng.uniform(0.06, 0.16)
            tr = seg * rng.uniform(0.06, 0.16)
            top, bot = [], []
            for u in us:
                k = min(smoothstep(0.0, tl, u - x), smoothstep(0.0, tr, x1 - u))
                k = 0.06 + 0.94 * float(k)
                mid = (y0 + y1) * 0.5 + wy(u)
                half = (y1 - y0) * 0.5 * k
                top.append((u, mid - half))
                bot.append((u, mid + half))
            blit(_jitter_ring(top + bot[::-1], rng, jit * 0.45), 255)
            x = x1 + rng.uniform(0.05, 0.16) * w

    # --- chunky bites out of the ink --------------------------------------
    for _ in range(int(rng.integers(16, 22))):
        cx = rng.uniform(0.0, w)
        cy = rng.uniform(-0.05, 1.05) * h
        blit(_chip(cx, cy, rng.uniform(0.045, 0.14) * h, rng), 0)

    # --- speed slivers torn off the top and bottom edges ------------------
    for _ in range(int(rng.integers(7, 11))):
        edge_top = rng.random() < 0.5
        y = (rng.uniform(-0.02, 0.10) if edge_top else rng.uniform(0.90, 1.02)) * h
        ang = np.deg2rad(rng.uniform(-7.0, 7.0))
        blit(_spike(rng.uniform(0.0, w), y,
                    rng.uniform(0.10, 0.22) * w, rng.uniform(0.07, 0.16) * h,
                    ang, flip=-1.0 if edge_top else 1.0), 255)

    # --- white scratch slivers cut back across the ink --------------------
    for _ in range(int(rng.integers(5, 8))):
        ang = np.deg2rad(rng.uniform(-6.0, 6.0))
        blit(_spike(rng.uniform(0.0, w), rng.uniform(0.0, 1.0) * h,
                    rng.uniform(0.12, 0.26) * w, rng.uniform(0.035, 0.075) * h,
                    ang, flip=1.0 if rng.random() < 0.5 else -1.0), 0)

    return np.asarray(img, dtype=np.float32) / 255.0


def contact_sheet(items):
    pad, scale = 28, 0.5
    tiles = []
    for name, m in items:
        img = Image.fromarray(((1.0 - m) * 255.0).astype(np.uint8), 'L').convert('RGB')
        tiles.append((name, img.resize((int(img.width * scale), int(img.height * scale)), Image.LANCZOS)))
    width = max(t.width for _, t in tiles) + pad * 2
    height = sum(t.height + pad for _, t in tiles) + pad
    sheet = Image.new('RGB', (width, height), (226, 226, 228))
    y = pad
    for _, t in tiles:
        sheet.paste(t, (pad, y))
        y += t.height + pad
    sheet.save(OUT / 'TireMark_ContactSheet.png')
    print(f'  contact sheet  {sheet.width}x{sheet.height}')


def main():
    print('Generating tire-mark masks ->', OUT)
    built = []
    jobs = [
        ('Tread_Heavy', variant_heavy, 2048, 512, 11),
        ('Tread_Worn', variant_worn, 2048, 512, 23),
        ('Toon_Worn', variant_toon_worn, 2048, 512, 29),
        ('Skid_Long', variant_skid, 4096, 512, 37),
        ('Patch_Short', variant_patch, 1024, 512, 51),
        ('Scuff_Drift', variant_scuff, 2048, 512, 67),
    ]
    for name, fn, w, h, seed in jobs:
        built.append((name, save(name, fn(w * SS, h * SS, seed))))
    contact_sheet(built)
    print('Done.')


if __name__ == '__main__':
    main()
