"""Draw the three placeholder forecast icons (white line art on transparent). Run with system Python + Pillow.

These are stand-ins so the forecast row works before final art exists; replace the PNGs or assign other
textures in the generator's Presentation > Forecast settings.
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw

OUT = Path(__file__).parent / 'Art'
SIZE, SS = 256, 4  # final edge, supersampling
N = SIZE * SS
STROKE = 15 * SS


def canvas():
    image = Image.new('L', (N, N), 0)
    return image, ImageDraw.Draw(image)


def pt(x, y):
    """Design space is -1..1 with y up."""
    return ((x * .5 + .5) * N, (.5 - y * .5) * N)


def stroke(draw, points, closed=False, width=STROKE):
    pts = [pt(*p) for p in points]
    if closed:
        pts.append(pts[0])
    draw.line(pts, fill=255, width=width, joint='curve')
    for x, y in pts:
        draw.ellipse((x - width / 2, y - width / 2, x + width / 2, y + width / 2), fill=255)


def finish(image, name):
    alpha = image.resize((SIZE, SIZE), Image.LANCZOS)
    white = Image.new('L', (SIZE, SIZE), 255)
    Image.merge('RGBA', (white, white, white, alpha)).save(OUT / name)


def slippery():
    image, draw = canvas()
    # Droplet over two skid waves.
    drop = [(0, .82)] + [(.36 * math.cos(math.radians(a)), .18 + .36 * math.sin(math.radians(a))) for a in range(35, -216, -5)]
    stroke(draw, drop, closed=True)
    for base in (-.48, -.76):
        stroke(draw, [(-.7 + 1.4 * i / 40, base + .07 * math.sin(i / 40 * math.tau * 1.5)) for i in range(41)], width=int(STROKE * .8))
    finish(image, 'T_ArenaForecast_Slippery.png')


def black_hole():
    image, draw = canvas()
    for phase in (0, 180):
        arm = []
        for i in range(61):
            t = i / 60
            a = math.radians(phase + 400 * t)
            r = .24 + .56 * t
            arm.append((r * math.cos(a), r * math.sin(a)))
        stroke(draw, arm)
    x0, y0 = pt(-.17, .17)
    x1, y1 = pt(.17, -.17)
    draw.ellipse((x0, y0, x1, y1), fill=255)
    finish(image, 'T_ArenaForecast_BlackHole.png')


def electric():
    image, draw = canvas()
    bolt = [(.18, .86), (-.42, .02), (-.04, .02), (-.2, -.86), (.44, .14), (.06, .14)]
    stroke(draw, bolt, closed=True)
    finish(image, 'T_ArenaForecast_Electric.png')


OUT.mkdir(exist_ok=True)
slippery()
black_hole()
electric()
print('wrote', sorted(p.name for p in OUT.glob('T_ArenaForecast_*.png')))
