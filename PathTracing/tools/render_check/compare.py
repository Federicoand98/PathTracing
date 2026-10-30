#!/usr/bin/env python3
"""Confronta due catture di capture.sh. Uso: compare.py <dir-a> <dir-b> [scene...]"""
import struct
import sys

import numpy as np


def load(path):
    data = open(path, "rb").read()
    w, h = struct.unpack("ii", data[:8])
    return np.frombuffer(data[8:], dtype=np.float32).reshape(h, w, 4)[..., :3]


a, b = sys.argv[1:3]
for s in sys.argv[3:] or ["2", "5", "6", "8"]:
    x, y = load(f"{a}/s{s}.hdr"), load(f"{b}/s{s}.hdr")
    mx, my = x.mean(), y.mean()
    # un'immagine nera confrontata con se stessa darebbe RMSE 0: sarebbe un falso positivo
    assert mx > 0 and my > 0, f"scena {s}: immagine nera"
    rmse = np.sqrt(((x - y) ** 2).mean())
    print(f"s{s}: media {mx:.4f} -> {my:.4f} ({(my / mx - 1) * 100:+.2f}%)  RMSE {rmse:.4f}  NaN {np.isnan(y).sum()}")
