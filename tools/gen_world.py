#!/usr/bin/env python3
# génère la montagne dans assets/world : relief (heightmap), route, cartes de sol, arbres et rochers
# usage : python3 tools/gen_world.py (numpy, scipy, Pillow)
# étapes : relief, tracé de la route, profil en long, terrassement autour de la route, cartes et objets
# la génération est déterministe (graine fixe)
import json, struct, sys, time
from pathlib import Path
import numpy as np
from scipy import ndimage as ndi
from scipy.spatial import cKDTree
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets/world'
OUT.mkdir(parents=True, exist_ok=True)

CELL = 1.5
N = 1345
HALF = (N - 1) * CELL / 2.0
HEIGHT_RANGE = 1200.0
ROAD_HALF_WIDTH = 3.25
SHOULDER = 0.7

rng = np.random.default_rng(20240607)
t0 = time.time()
def log(*a): print('[%5.1fs]' % (time.time() - t0), *a, flush=True)

xs = (np.arange(N) * CELL - HALF).astype(np.float64)
X, Z = np.meshgrid(xs, xs)

def lattice_noise(Xc, Zc, wavelength, seed, aniso=(1.0, 1.0)):
    r = np.random.default_rng(seed)
    wx, wz = wavelength * aniso[0], wavelength * aniso[1]
    nx = int(np.ceil(2 * HALF * 1.5 / wx)) + 6
    nz = int(np.ceil(2 * HALF * 1.5 / wz)) + 6
    lat = r.uniform(-1, 1, (nz, nx))
    cx = (Xc + HALF * 1.25) / wx + 2
    cz = (Zc + HALF * 1.25) / wz + 2
    return np.clip(ndi.map_coordinates(lat, [cz, cx], order=3, mode='reflect'), -1, 1)

def fbm(Xc, Zc, wl, octaves, gain, seed, aniso=(1.0, 1.0), ridged=False):
    total = np.zeros_like(Xc); amp = 1.0; norm = 0.0; weight = np.ones_like(Xc)
    for o in range(octaves):
        n = lattice_noise(Xc, Zc, wl, seed + o * 17, aniso)
        if ridged:
            n = (1.0 - (np.sqrt(n * n + 0.05) - 0.2236) / 0.8)
            n = np.clip(n, 0, 1) ** 2
            n = n * weight
            weight = np.clip(n * 2.0, 0, 1)
        total += n * amp; norm += amp
        amp *= gain; wl *= 0.5
    return total / norm

def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)

def build_path():
    R_MIN, R_MAX = 14.5, 19.0
    ctrl = []
    heading = -1.0
    x, z = 400.0, -560.0
    for dx in (160.0, 110.0, 60.0, 0.0):
        ctrl.append((x - 0.0 + dx - 0.0, z - 0.1 * dx, 0))

    legs = [(330, 0.095), (410, 0.11), (300, 0.09), (460, 0.12), (350, 0.10), (270, 0.11),
            (430, 0.095), (330, 0.12), (390, 0.10), (300, 0.11), (420, 0.09), (310, 0.11)]
    for (Lx, slope) in legs:
        R = rng.uniform(R_MIN, R_MAX)
        end_x = float(np.clip(x + heading * Lx, -330, 400))
        Lx_eff = abs(end_x - x)
        n = max(3, int(Lx_eff / 55))
        f = rng.uniform(1.2, 2.6); phi = rng.uniform(0, 6.28); A = rng.uniform(5.0, 13.0)
        for k in range(1, n):
            t = k / n
            ctrl.append((x + heading * Lx_eff * t,
                         z + slope * Lx_eff * t + A * np.sin(2 * np.pi * f * t + phi) * np.sin(np.pi * t) ** 0.8, 0))
        ex, ez = end_x, z + slope * Lx_eff
        ctrl.append((ex - heading * 16.0, ez - slope * 16.0, 0))
        for tt in np.linspace(0.0, np.pi, 9):
            ctrl.append((ex + heading * R * np.sin(tt), ez + R - R * np.cos(tt), 1))
        x, z = ex, ez + 2 * R
        heading = -heading
        ctrl.append((x + heading * 16.0, z + slope * 16.0, 0))

    for k in range(1, 9):
        t = k / 8
        ctrl.append((x + heading * 55 * k + 35 * np.sin(1.7 * np.pi * t) * heading, z + 50 * k + 18 * np.sin(2.4 * np.pi * t), 0))
    return ctrl

def catmull_rom_centripetal(P, spacing=1.0):
    P = np.asarray(P, dtype=np.float64)
    pts = [P[0] + (P[0] - P[1])] + list(P) + [P[-1] + (P[-1] - P[-2])]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[i + 1], pts[i + 2]
        tj = lambda ti, a, b: ti + max(np.linalg.norm(b - a), 1e-6) ** 0.5
        t0_ = 0.0; t1_ = tj(t0_, p0, p1); t2_ = tj(t1_, p1, p2); t3_ = tj(t2_, p2, p3)
        nseg = max(6, int(np.linalg.norm(p2 - p1) / 0.7))
        for t in np.linspace(t1_, t2_, nseg, endpoint=False):
            A1 = (t1_ - t) / (t1_ - t0_) * p0 + (t - t0_) / (t1_ - t0_) * p1
            A2 = (t2_ - t) / (t2_ - t1_) * p1 + (t - t1_) / (t2_ - t1_) * p2
            A3 = (t3_ - t) / (t3_ - t2_) * p2 + (t - t2_) / (t3_ - t2_) * p3
            B1 = (t2_ - t) / (t2_ - t0_) * A1 + (t - t0_) / (t2_ - t0_) * A2
            B2 = (t3_ - t) / (t3_ - t1_) * A2 + (t - t1_) / (t3_ - t1_) * A3
            out.append((t2_ - t) / (t2_ - t1_) * B1 + (t - t1_) / (t2_ - t1_) * B2)
    out.append(P[-1])
    out = np.array(out)
    d = np.concatenate([[0], np.cumsum(np.linalg.norm(np.diff(out, axis=0), axis=1))])
    s = np.arange(0, d[-1], spacing)
    return np.stack([np.interp(s, d, out[:, 0]), np.interp(s, d, out[:, 1])], axis=1)

def thermal_erosion(H, iters, talus, rate=0.2):
    for _ in range(iters):
        delta = np.zeros_like(H)
        for axis, shift in ((0, 1), (0, -1), (1, 1), (1, -1)):
            Hn = np.roll(H, shift, axis=axis)
            excess = np.maximum(H - Hn - talus * CELL, 0.0)
            amount = rate * 0.25 * excess
            delta -= amount
            delta += np.roll(amount, -shift, axis=axis)
        H += delta
    return H

def natural_terrain(P):
    log('relief naturel...')

    s_ = np.arange(len(P), dtype=np.float64)
    h_uniform = 585.0 - 0.078 * s_

    zbins = np.arange(-1100.0, 1100.0, 5.0)
    which = np.clip(((P[:, 1] + 1100.0) / 5.0).astype(int), 0, len(zbins) - 1)
    sums = np.bincount(which, weights=h_uniform, minlength=len(zbins))
    cnts = np.bincount(which, minlength=len(zbins))
    have = cnts > 0
    prof = np.interp(zbins, zbins[have], sums[have] / cnts[have])
    z_first, z_last = P[:, 1].min(), P[:, 1].max()
    north = zbins < z_first
    south = zbins > z_last
    prof[north] = prof[have][0] + (z_first - zbins[north]) * 0.28
    prof[south] = np.maximum(prof[have][-1] - (zbins[south] - z_last) * 0.12, 100.0)
    prof = ndi.gaussian_filter1d(prof, 16)
    base = np.interp(Z, zbins, prof)

    warp = 110.0
    Xw = X + warp * fbm(X, Z, 560.0, 2, 0.5, 11)
    Zw = Z + warp * fbm(X, Z, 560.0, 2, 0.5, 31)
    corridor = 1.0 - smoothstep(420.0, 700.0, np.abs(X - 40.0))
    ranges = fbm(Xw, Zw, 850.0, 4, 0.5, 100, aniso=(1.0, 1.5), ridged=True)
    summit = smoothstep(-640, -1000, Z)
    H = base + (1.0 - corridor) * (190.0 * (ranges - 0.2) + 35.0) + summit * (110.0 + 70.0 * ranges)

    rr = np.maximum(np.abs(X + 60 * fbm(X, Z, 300.0, 2, 0.5, 51)), np.abs(Z + 60 * fbm(X, Z, 300.0, 2, 0.5, 71))) / HALF
    rim = smoothstep(0.80, 1.0, rr)
    H += rim * (170.0 + 110.0 * ranges)

    face = 0.55 + 0.45 * (1.0 - corridor)
    ridges2 = fbm(Xw, Zw, 230.0, 5, 0.52, 300, aniso=(1.0, 2.0), ridged=True)
    detail = fbm(Xw, Zw, 70.0, 5, 0.5, 400, aniso=(1.0, 1.6))
    H += face * (34.0 * (ridges2 - 0.3) + 8.0 * detail)
    H = thermal_erosion(H, 24, 0.9)
    return H

def bilinear(H, x, z):
    fx = (np.asarray(x) + HALF) / CELL
    fz = (np.asarray(z) + HALF) / CELL
    return ndi.map_coordinates(H, [fz, fx], order=1, mode='nearest')

def road_geometry(P):
    f = np.gradient(P, axis=0)
    f /= np.linalg.norm(f, axis=1, keepdims=True)
    left = np.stack([f[:, 1], -f[:, 0]], axis=1)
    theta = np.unwrap(np.arctan2(f[:, 0], f[:, 1]))
    kappa = ndi.gaussian_filter1d(np.gradient(theta), 2.0)
    return f, left, kappa

def fit_profile(T0, P, kappa):
    h = bilinear(T0, P[:, 0], P[:, 1])
    h = ndi.gaussian_filter1d(h, 70, mode='nearest')
    hairpin = ndi.maximum_filter1d((np.abs(kappa) > 1 / 30.0).astype(float), 25) > 0.5
    gmax = np.where(hairpin, 0.075, 0.105)
    for _ in range(60):
        for i in range(len(h) - 1):
            h[i + 1] = min(max(h[i + 1], h[i] - gmax[i]), h[i] + gmax[i])
        for i in range(len(h) - 2, -1, -1):
            h[i] = min(max(h[i], h[i + 1] - gmax[i]), h[i + 1] + gmax[i])
    h = ndi.gaussian_filter1d(h, 6, mode='nearest')
    return h, hairpin

def carve(T0, P, f, left, kappa, h, hairpin):
    log('terrassement...')
    M = len(P)
    s = np.arange(M, dtype=np.float64)

    width = np.full(M, ROAD_HALF_WIDTH) + ndi.gaussian_filter1d(hairpin.astype(float), 6) * 0.8

    start_w = 1.0 - smoothstep(55, 85, s)
    width_carve = width + start_w * 5.5
    edge = width_carve + SHOULDER

    bank = np.clip(0.9 * kappa, -0.065, 0.065)

    nz = lattice_noise(s[None, :] * 4.0, np.zeros((1, M)), 60.0, 901)[0]
    nh = lattice_noise(s[None, :] * 4.0, np.zeros((1, M)), 140.0, 902)[0]
    cut_slope = 1.5 + 0.9 * nz
    wall_h = 3.5 + 9.5 * np.clip(0.5 + 0.9 * nh, 0, 1) ** 1.4
    tree = cKDTree(P)
    d, idx = tree.query(np.stack([X.ravel(), Z.ravel()], axis=1), k=1, workers=-1)
    d = d.reshape(N, N); idx = idx.reshape(N, N)
    px = P[idx, 0]; pz = P[idx, 1]
    u = (X - px) * left[idx, 0] + (Z - pz) * left[idx, 1]
    e = edge[idx]
    surf = h[idx] + bank[idx] * np.clip(u, -e, e)
    dd = np.maximum(d - e, 0.0)
    he = h[idx] + bank[idx] * np.sign(u) * e
    rough = fbm(X, Z, 9.0, 3, 0.55, 777, ridged=True)
    dd_eff = dd * (1.0 + 0.45 * (rough - 0.4))
    tc = cut_slope[idx]; Hw = wall_h[idx]
    up = he + np.minimum(tc * dd_eff, Hw) + 0.72 * np.maximum(dd_eff - Hw / tc, 0.0)
    down = he - 0.62 * dd
    T = np.clip(T0, down, up)
    cut_mask = (T0 > up + 0.05) & (dd > 0)
    fill_mask = (T0 < down - 0.05) & (dd > 0)
    on_road = d <= e
    T = np.where(on_road, surf - 0.14, T)

    Ts = ndi.gaussian_filter(T, 1.0)
    w = smoothstep(0.0, 3.0, d - e)
    T = T * (1 - w) + Ts * w
    Tfar = ndi.gaussian_filter(T, 3.0)
    T = T * (1 - smoothstep(60.0, 160.0, d)) + Tfar * smoothstep(60.0, 160.0, d)
    return T, cut_mask, fill_mask, d, u, idx, width, bank, edge, e

def sample(T, x, z):
    return bilinear(T, x, z)

def runs(mask):
    m = np.concatenate([[0], mask.astype(int), [0]])
    dm = np.diff(m)
    return list(zip(np.nonzero(dm == 1)[0], np.nonzero(dm == -1)[0]))

def export_all(T, P, f, left, kappa, h, hairpin, width, bank, edge, cut_mask, fill_mask, d, e):
    M = len(P)
    log('exports...')

    h16 = np.clip(T / HEIGHT_RANGE * 65535.0, 0, 65535).astype(np.uint16)
    Image.fromarray(h16).save(OUT / 'heightmap.png')

    gy, gx = np.gradient(T, CELL)
    slope = np.arctan(np.hypot(gx, gy))
    dist_edge = d - e

    R_ = 1.0 - smoothstep(0.3, 6.5, dist_edge)
    forest = 0.5 + 0.5 * fbm(X, Z, 90.0, 3, 0.5, 4242)
    G_ = np.clip((forest - 0.30) * 3.0, 0, 1) * smoothstep(4.0, 12.0, dist_edge)
    B_ = np.clip(smoothstep(np.radians(34), np.radians(52), slope) + 0.8 * cut_mask + 0.35 * smoothstep(620, 900, T), 0, 1)
    A_ = np.clip(0.5 + 0.9 * fbm(X, Z, 45.0, 3, 0.55, 5252), 0, 1)
    ctrl = np.stack([R_, G_, B_, A_], axis=-1)
    Image.fromarray((ctrl * 255).astype(np.uint8), 'RGBA').save(OUT / 'control.png')

    flags = np.zeros(M, dtype=np.int32)
    rail = {}
    for side, bit in ((+1, 2), (-1, 4)):
        q = P + side * left * (edge[:, None] + 3.0)
        he = h + bank * side * edge
        drop = he - sample(T, q[:, 0], q[:, 1])
        outer = hairpin & ((kappa * side) < 0)
        m = (drop > 1.8) | outer
        m = ndi.binary_closing(m, np.ones(25))
        m = ndi.binary_opening(m, np.ones(14))
        rail[side] = m
        flags |= np.where(m, bit, 0)
    flags |= np.where(hairpin, 1, 0)

    lamp_idx, sign_idx = [], []
    for k, (a_, b_) in enumerate(runs(hairpin)):
        apex = (a_ + b_) // 2
        if k % 2 == 0:
            lamp_idx.append(apex)
        for si in np.linspace(a_ + 8, b_ - 8, 5).astype(int):
            sign_idx.append(si)
    lamp_idx = [30, 70] + lamp_idx + [M - 120]
    for i in lamp_idx: flags[i] |= 8
    for i in sign_idx: flags[i] |= 16
    flags[:75] |= 32
    flags[M - 80:] |= 64

    with open(OUT / 'road.csv', 'w') as fh:
        fh.write('# x,y,z,largeur_demi,devers,courbure,flags\n')
        for i in range(M):
            fh.write('%.3f,%.3f,%.3f,%.3f,%.4f,%.5f,%d\n' % (P[i, 0], h[i], P[i, 1], width[i], bank[i], kappa[i], flags[i]))
    log('route.csv : %d échantillons, garde-corps gauche %.0f m / droite %.0f m, %d lampadaires, %d panneaux' % (
        M, rail[1].sum(), rail[-1].sum(), len(lamp_idx), len(sign_idx)))

    log('arbres...')
    r2 = np.random.default_rng(777)
    cand = r2.uniform(-HALF + 5, HALF - 5, (420000, 2))
    ct = bilinear(T, cand[:, 0], cand[:, 1])
    cs = bilinear(slope, cand[:, 0], cand[:, 1])
    cd = bilinear(dist_edge, cand[:, 0], cand[:, 1])
    cf = bilinear(forest, cand[:, 0], cand[:, 1])
    cc = bilinear(cut_mask.astype(float), cand[:, 0], cand[:, 1])
    p = (1.0 - smoothstep(860, 1000, ct)) * (1.0 - smoothstep(np.radians(38), np.radians(52), cs))
    p *= smoothstep(4.5, 10.0, cd) * (1.0 - cc)
    p *= 0.25 + 0.75 * smoothstep(0.22, 0.5, cf)
    keep = r2.uniform(0, 1, len(cand)) < p * 0.62
    pts = cand[keep]
    log('  candidats retenus : %d' % len(pts))
    tree = cKDTree(pts)
    alive = np.ones(len(pts), bool)
    for i, j in tree.query_pairs(4.4, output_type='ndarray'):
        if alive[i] and alive[j]:
            alive[j] = False
    pts = pts[alive]
    ty = bilinear(T, pts[:, 0], pts[:, 1])
    variants = r2.choice(6, size=len(pts), p=[0.08, 0.17, 0.20, 0.20, 0.20, 0.15])
    scales = r2.uniform(0.82, 1.18, len(pts))
    yaws = r2.uniform(0, 2 * np.pi, len(pts))
    rec = np.zeros(len(pts), dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('yaw', '<f4'), ('scale', '<f4'), ('variant', '<u4')])
    rec['x'], rec['y'], rec['z'], rec['yaw'], rec['scale'], rec['variant'] = pts[:, 0], ty - 0.12, pts[:, 1], yaws, scales, variants
    rec.tofile(OUT / 'trees.bin')
    log('  arbres : %d' % len(pts))

    rk = r2.uniform(-HALF + 5, HALF - 5, (260000, 2))
    rd = bilinear(dist_edge, rk[:, 0], rk[:, 1]); rc = bilinear(cut_mask.astype(float), rk[:, 0], rk[:, 1])
    rs = bilinear(slope, rk[:, 0], rk[:, 1]); rt = bilinear(T, rk[:, 0], rk[:, 1])
    pr = (rd < 40) * (0.5 + 1.2 * rc) * (rd > 1.0) * 0.03 + (rs > np.radians(38)) * 0.004
    rk = rk[r2.uniform(0, 1, len(rk)) < pr]
    ry = bilinear(T, rk[:, 0], rk[:, 1])
    rrec = np.zeros(len(rk), dtype=[('x', '<f4'), ('y', '<f4'), ('z', '<f4'), ('yaw', '<f4'), ('scale', '<f4'), ('variant', '<u4')])
    rrec['x'], rrec['y'], rrec['z'] = rk[:, 0], ry - 0.15, rk[:, 1]
    rrec['yaw'] = r2.uniform(0, 2 * np.pi, len(rk)); rrec['scale'] = r2.uniform(0.6, 2.2, len(rk)); rrec['variant'] = r2.choice(3, len(rk))
    rrec.tofile(OUT / 'rocks.bin')
    log('  rochers : %d' % len(rk))

    with open(OUT / 'world.cfg', 'w') as fh:
        fh.write('grid=%d\ncell=%.4f\nheight_range=%.1f\nroad_samples=%d\ntrees=%d\nrocks=%d\n' % (N, CELL, HEIGHT_RANGE, M, len(pts), len(rk)))
    return slope, pts

if __name__ == '__main__':
    ctrl = build_path()
    P = catmull_rom_centripetal([(c[0], c[1]) for c in ctrl], 1.0)
    T0 = natural_terrain(P)
    f, left, kappa = road_geometry(P)
    h, hairpin = fit_profile(T0, P, kappa)
    natural = bilinear(T0, P[:, 0], P[:, 1])
    log('route : %d m ; altitude %.0f -> %.0f m ; écart au terrain naturel : cut max %.1f m, fill max %.1f m' % (
        len(P), h[0], h[-1], (natural - h).max(), (h - natural).max()))
    g = np.gradient(h)
    log('pente : max %.1f %%, moyenne descente %.1f %%' % (100 * np.abs(g).max(), 100 * (h[0] - h[-1]) / len(P)))
    Rm = 1 / np.maximum(np.abs(kappa), 1e-9)
    log('rayon minimal %.1f m' % Rm.min())
    T, cut_mask, fill_mask, d, u, idx, width, bank, edge, e = carve(T0, P, f, left, kappa, h, hairpin)
    log('terrain : %.0f..%.0f m ; déblais %.1f %% ; remblais %.1f %% de la surface' % (
        T.min(), T.max(), 100 * cut_mask.mean(), 100 * fill_mask.mean()))
    slope, trees = export_all(T, P, f, left, kappa, h, hairpin, width, bank, edge, cut_mask, fill_mask, d, e)
    np.savez(OUT / '_state.npz', T=T, P=P, h=h, kappa=kappa, hairpin=hairpin, width=width, bank=bank, edge=edge, trees=trees)
