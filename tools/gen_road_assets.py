#!/usr/bin/env python3
# génère les textures de la route dans assets/textures : marquages, chevrons et ligne d'arrivée
# usage : python3 tools/gen_road_assets.py (numpy, Pillow)
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets/textures'
rng = np.random.default_rng(5)

def blur_noise(h, w, sigma, seed):
    r = np.random.default_rng(seed)
    n = r.normal(size=(h, w))
    img = Image.fromarray(((n - n.min()) / (n.max() - n.min()) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(sigma))
    a = np.asarray(img, dtype=np.float32) / 255.0
    return (a - a.min()) / (a.max() - a.min())

def road_markings():
    W, H = 256, 1024
    img = np.zeros((H, W, 4), dtype=np.float32)
    def line(u0, width, color):
        x0, x1 = int((u0 - width / 2) * W), int((u0 + width / 2) * W)
        img[:, x0:x1, :3] = color
        img[:, x0:x1, 3] = 1.0
    white = (0.90, 0.90, 0.86); yellow = (0.95, 0.72, 0.10)
    line(0.045, 0.022, white)
    line(0.955, 0.022, white)
    line(0.5, 0.024, yellow)

    wear = blur_noise(H, W, 3.0, 1) * 0.65 + blur_noise(H, W, 12.0, 2) * 0.35
    img[..., 3] *= np.clip((wear - 0.30) * 4.0, 0, 1) * 0.5 + 0.5
    img[..., 3] *= 0.92
    Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8), 'RGBA').save(OUT / 'road_markings.png')

def chevron():
    W, H = 256, 128
    im = Image.new('RGBA', (W, H), (240, 190, 0, 255))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W - 1, H - 1], outline=(10, 10, 10, 255), width=7)
    for cx in (84, 170):
        d.polygon([(cx - 28, 20), (cx, 20), (cx + 32, H // 2), (cx, H - 20), (cx - 28, H - 20), (cx + 4, H // 2)], fill=(15, 15, 15, 255))
    im.save(OUT / 'chevron_right.png')
    im.transpose(Image.FLIP_LEFT_RIGHT).save(OUT / 'chevron_left.png')

def finish_line():
    im = np.zeros((64, 512, 3), dtype=np.uint8)
    for y in range(64):
        for x in range(512):
            im[y, x] = 235 if ((x // 32) + (y // 32)) % 2 == 0 else 20
    Image.fromarray(im).save(OUT / 'finish_line.png')

if __name__ == '__main__':
    road_markings(); chevron(); finish_line()
    print('textures générées dans', OUT)
