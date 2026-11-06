#!/usr/bin/env python3
"""Draws docs/images/progress.svg, the progress graphic of the README, from tools/progress/progress.json.

usage: tools/progress/progress.py [progress.json] [out.svg]

A milestone's percent is an estimate of how much of it is done; a step is the weighted average of its milestones,
the whole project the weighted average of the steps. Change the JSON when a milestone moves, then run this.
The SVG has its own dark background, so it reads the same on light and dark pages.
"""
import json
import math
import os
import sys
from xml.sax.saxutils import escape

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'tools', 'progress', 'progress.json')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'docs', 'images', 'progress.svg')

BG, PANEL, LINE = '#1b1814', '#25211b', '#3a342b'
TEXT, MUTED = '#f1e9d8', '#a89c86'
STEP_COLORS = ['#e7b85a', '#d9603f', '#8fb3c9']
STATUS = {'done': ('#7fbf6a', 'done'), 'in progress': ('#e7b85a', 'in progress'), 'not started': ('#8a7f6c', 'not started')}
FONT = "font-family=\"'Segoe UI', 'Helvetica Neue', Arial, sans-serif\""
SERIF = "font-family=\"'Palatino Linotype', 'Book Antiqua', Palatino, Georgia, serif\""


def weighted(items, value, weight):
    total = sum(weight(i) for i in items)
    return sum(weight(i) * value(i) for i in items) / total if total else 0


def main():
    data = json.load(open(SOURCE))
    steps = data['steps']
    for s in steps:
        s['percent'] = weighted(s['milestones'], lambda m: m['percent'], lambda m: m['weight'])
    overall = weighted(steps, lambda s: s['percent'], lambda s: s['weight'])

    width = 920
    rows = sum(len(s['milestones']) for s in steps)
    top_h = max(230, 100 + len(steps) * 46 + 10)  # the ring, or the step bars when there are many
    row_h = 26
    height = top_h + 40 + len(steps) * 34 + rows * row_h + 56
    out = []
    add = out.append
    add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="{width}" height="{height}">')
    add(f'<title>{escape(data.get("title", "Progress"))}</title>')
    add(f'<rect width="{width}" height="{height}" rx="14" fill="{BG}"/>')

    # the ring: the whole project
    cx, cy, r, w = 130, 125, 78, 16
    add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{LINE}" stroke-width="{w}"/>')
    angle = 2 * math.pi * max(min(overall / 100, 1), 0.002)
    x1, y1 = cx, cy - r
    x2, y2 = cx + r * math.sin(angle), cy - r * math.cos(angle)
    large = 1 if angle > math.pi else 0
    add(f'<path d="M{x1:.1f},{y1:.1f} A{r},{r} 0 {large} 1 {x2:.1f},{y2:.1f}" fill="none" stroke="{STEP_COLORS[0]}" '
        f'stroke-width="{w}" stroke-linecap="round"/>')
    add(f'<text x="{cx}" y="{cy + 12}" text-anchor="middle" {SERIF} font-size="44" fill="{TEXT}">{round(overall)}%</text>')
    add(f'<text x="{cx}" y="{cy + 36}" text-anchor="middle" {FONT} font-size="13" fill="{MUTED}">of the whole project</text>')

    # the steps
    x0, bar_w = 260, 620
    add(f'<text x="{x0}" y="44" {FONT} font-size="18" font-weight="600" fill="{TEXT}">Progress</text>')
    add(f'<text x="{x0}" y="64" {FONT} font-size="12" fill="{MUTED}">State of {escape(data["date"])}. '
        'Percentages are estimates, steps weighted by their share of the work.</text>')
    for i, s in enumerate(steps):
        y = 100 + i * 46
        color = STEP_COLORS[i % len(STEP_COLORS)]
        add(f'<text x="{x0}" y="{y}" {FONT} font-size="14" font-weight="600" fill="{TEXT}">{s["step"]}. {escape(s["name"])}'
            f'<tspan dx="10" font-weight="400" fill="{MUTED}" font-size="12">{escape(s["detail"])}, weight {s["weight"]}</tspan></text>')
        add(f'<text x="{x0 + bar_w}" y="{y}" text-anchor="end" {SERIF} font-size="18" fill="{TEXT}">{round(s["percent"])}%</text>')
        add(f'<rect x="{x0}" y="{y + 9}" width="{bar_w}" height="10" rx="5" fill="{LINE}"/>')
        add(f'<rect x="{x0}" y="{y + 9}" width="{max(bar_w * s["percent"] / 100, 4):.1f}" height="10" rx="5" fill="{color}"/>')

    # the milestones
    y = top_h + 30
    add(f'<line x1="30" y1="{top_h}" x2="{width - 30}" y2="{top_h}" stroke="{LINE}"/>')
    add(f'<text x="30" y="{y}" {FONT} font-size="15" font-weight="600" fill="{TEXT}">Milestones</text>')
    y += 10
    col_item, col_status, col_bar, col_pct, col_note = 30, 290, 385, 485, 520
    for i, s in enumerate(steps):
        y += 30
        color = STEP_COLORS[i % len(STEP_COLORS)]
        add(f'<text x="{col_item}" y="{y}" {FONT} font-size="12" font-weight="600" fill="{color}">'
            f'{s["step"]}. {escape(s["name"]).upper()}</text>')
        y += 4
        for m in s['milestones']:
            y += row_h
            scolor, slabel = STATUS.get(m['status'], STATUS['not started'])
            add(f'<text x="{col_item}" y="{y}" {FONT} font-size="13" fill="{TEXT}">{escape(m["item"])}</text>')
            add(f'<rect x="{col_status}" y="{y - 13}" width="84" height="18" rx="9" fill="none" stroke="{scolor}"/>')
            add(f'<text x="{col_status + 42}" y="{y}" text-anchor="middle" {FONT} font-size="11" fill="{scolor}">{slabel}</text>')
            add(f'<rect x="{col_bar}" y="{y - 8}" width="90" height="6" rx="3" fill="{LINE}"/>')
            if m['percent'] > 0:
                add(f'<rect x="{col_bar}" y="{y - 8}" width="{max(90 * m["percent"] / 100, 3):.1f}" height="6" rx="3" fill="{color}"/>')
            add(f'<text x="{col_pct + 26}" y="{y}" text-anchor="end" {FONT} font-size="12" fill="{TEXT}">{m["percent"]}%</text>')
            add(f'<text x="{col_note}" y="{y}" {FONT} font-size="11.5" fill="{MUTED}">{escape(m["note"])}</text>')
    add('</svg>')
    svg = '\n'.join(out) + '\n'
    # the real height: the last row plus a margin
    real = y + 28
    svg = svg.replace(f'viewBox="0 0 {width} {height}" width="{width}" height="{height}"',
                      f'viewBox="0 0 {width} {real}" width="{width}" height="{real}"', 1)
    svg = svg.replace(f'<rect width="{width}" height="{height}" rx="14"', f'<rect width="{width}" height="{real}" rx="14"', 1)
    open(OUT, 'w').write(svg)
    print(f'{OUT}: {round(overall)}% overall, ' + ', '.join(f'{s["name"]} {round(s["percent"])}%' for s in steps))


main()
