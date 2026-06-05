#!/usr/bin/env python3
"""
Plot benchmark results for exec_deferred_mmput optimization.
Generates publication-quality figures for the CS614 report.

Usage:
    python3 plot_results.py baseline.csv optimized.csv [output_dir]

CSV format expected (from benchmark.sh):
    test,bss_mb,total_runs,raw_avg_execve_ns,...,filtered_avg_execve_ns,...,filtered_avg_mmput_ns,...
"""

import sys
import os
import csv
import matplotlib
matplotlib.use('Agg')  # non-interactive backend
import matplotlib.pyplot as plt
import numpy as np

# ── Parse arguments ──────────────────────────────────────────────
if len(sys.argv) < 3:
    print("Usage: python3 plot_results.py baseline.csv optimized.csv [output_dir]")
    sys.exit(1)

baseline_csv = sys.argv[1]
optimized_csv = sys.argv[2]
output_dir = sys.argv[3] if len(sys.argv) > 3 else "."

os.makedirs(output_dir, exist_ok=True)

# ── Parse CSV ────────────────────────────────────────────────────
def parse_csv(filepath):
    """Parse benchmark CSV, return dict of test_name -> {bss, execve_us, deferred_us}"""
    results = []
    with open(filepath, 'r') as f:
        reader = csv.reader(f)
        for row in reader:
            # Skip comments, headers, raw data rows
            if not row or row[0].startswith('#') or row[0] == 'test':
                continue
            # Summary rows have 13 columns
            if len(row) >= 13:
                try:
                    name = row[0].strip()
                    bss = int(row[1].strip())
                    # filtered_avg_execve_us = col 10 (0-indexed: 9)
                    execve_us = float(row[9].strip())
                    # filtered_avg_mmput_us = col 13 (0-indexed: 12)
                    deferred_us = float(row[12].strip())
                    results.append({
                        'name': name,
                        'bss_mb': bss,
                        'execve_us': execve_us,
                        'deferred_us': deferred_us,
                    })
                except (ValueError, IndexError):
                    continue
    return results

baseline = parse_csv(baseline_csv)
optimized = parse_csv(optimized_csv)

if not baseline or not optimized:
    print("ERROR: Could not parse CSV files. Check format.")
    print(f"  Baseline rows: {len(baseline)}")
    print(f"  Optimized rows: {len(optimized)}")
    sys.exit(1)

print(f"Parsed {len(baseline)} baseline entries, {len(optimized)} optimized entries")

# ── Extract arrays ───────────────────────────────────────────────
bss_sizes = [r['bss_mb'] for r in baseline]
base_execve = [r['execve_us'] for r in baseline]
base_deferred = [r['deferred_us'] for r in baseline]
opt_execve = [r['execve_us'] for r in optimized]
opt_deferred = [r['deferred_us'] for r in optimized]

labels = [f"{b} MB" if b > 0 else "0 MB" for b in bss_sizes]

# ── Style ────────────────────────────────────────────────────────
plt.rcParams.update({
    'font.family': 'serif',
    'font.size': 11,
    'axes.titlesize': 13,
    'axes.labelsize': 12,
    'legend.fontsize': 10,
    'figure.dpi': 150,
    'savefig.dpi': 300,
})

BLUE = '#2563eb'
RED = '#dc2626'
GREEN = '#16a34a'
ORANGE = '#ea580c'
GRAY = '#6b7280'

# ══════════════════════════════════════════════════════════════════
# FIGURE 1: Execve Latency Comparison (Bar Chart)
# ══════════════════════════════════════════════════════════════════
fig, ax = plt.subplots(figsize=(10, 5))
x = np.arange(len(labels))
width = 0.35

bars1 = ax.bar(x - width/2, base_execve, width, label='Baseline (sync)',
               color=RED, alpha=0.85, edgecolor='white', linewidth=0.5)
bars2 = ax.bar(x + width/2, opt_execve, width, label='Optimized (async)',
               color=GREEN, alpha=0.85, edgecolor='white', linewidth=0.5)

ax.set_xlabel('Process Size (BSS)')
ax.set_ylabel('Average execve Latency (μs)')
ax.set_title('execve() Latency: Baseline vs Async Teardown')
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=45, ha='right')
ax.legend()
ax.set_yscale('log')
ax.grid(axis='y', alpha=0.3)
ax.set_axisbelow(True)

fig.tight_layout()
path1 = os.path.join(output_dir, 'fig1_execve_comparison.png')
fig.savefig(path1)
print(f"Saved: {path1}")
plt.close()

# ══════════════════════════════════════════════════════════════════
# FIGURE 2: Sync Cost (deferred_mmput time) Comparison
# ══════════════════════════════════════════════════════════════════
fig, ax = plt.subplots(figsize=(10, 5))

ax.plot(bss_sizes, base_deferred, 'o-', color=RED, linewidth=2,
        markersize=6, label='Baseline: __mmput (sync)')
ax.plot(bss_sizes, opt_deferred, 's-', color=GREEN, linewidth=2,
        markersize=6, label='Optimized: Phase 1 only (async)')

ax.set_xlabel('Process Size (MB)')
ax.set_ylabel('Synchronous Cost (μs)')
ax.set_title('Synchronous Cost of exec_deferred_mmput: O(n) → O(1)')
ax.legend()
ax.grid(alpha=0.3)
ax.set_axisbelow(True)

# Annotate the O(1) line
if opt_deferred:
    avg_opt = np.mean(opt_deferred[1:])  # exclude 0MB (sync fallback)
    ax.axhline(y=avg_opt, color=GREEN, linestyle='--', alpha=0.5)
    ax.annotate(f'~{avg_opt:.1f} μs (O(1))',
                xy=(bss_sizes[-1], avg_opt),
                xytext=(bss_sizes[-1] * 0.7, avg_opt * 3),
                arrowprops=dict(arrowstyle='->', color=GREEN),
                color=GREEN, fontweight='bold')

fig.tight_layout()
path2 = os.path.join(output_dir, 'fig2_sync_cost.png')
fig.savefig(path2)
print(f"Saved: {path2}")
plt.close()

# ══════════════════════════════════════════════════════════════════
# FIGURE 3: Speedup Factor
# ══════════════════════════════════════════════════════════════════
fig, ax = plt.subplots(figsize=(10, 5))

speedup = []
for b, o in zip(base_execve, opt_execve):
    speedup.append(b / o if o > 0 else 0)

ax.bar(x, speedup, color=BLUE, alpha=0.85, edgecolor='white', linewidth=0.5)
ax.set_xlabel('Process Size (BSS)')
ax.set_ylabel('Speedup (×)')
ax.set_title('execve() Speedup: Baseline / Optimized')
ax.set_xticks(x)
ax.set_xticklabels(labels, rotation=45, ha='right')
ax.grid(axis='y', alpha=0.3)
ax.set_axisbelow(True)

# Add value labels on bars
for i, s in enumerate(speedup):
    if s > 1:
        ax.text(i, s + max(speedup)*0.02, f'{s:.0f}×',
                ha='center', va='bottom', fontweight='bold', fontsize=9)

fig.tight_layout()
path3 = os.path.join(output_dir, 'fig3_speedup.png')
fig.savefig(path3)
print(f"Saved: {path3}")
plt.close()

# ══════════════════════════════════════════════════════════════════
# FIGURE 4: Timeline Diagram (execve vs background teardown)
# ══════════════════════════════════════════════════════════════════
fig, axes = plt.subplots(2, 1, figsize=(10, 5.5), sharex=True)
fig.subplots_adjust(hspace=0.5)

# Pick the largest test that successfully optimized (where sync cost dropped > 50%)
idx = -1
for i in range(len(bss_sizes)-1, -1, -1):
    if opt_deferred[i] > 0 and (base_deferred[i] / opt_deferred[i]) > 100:
        idx = i
        break
if idx == -1:
    idx = -2 # Fallback if none found

largest_bss = bss_sizes[idx]
base_exec_t = base_execve[idx]
base_defer_t = base_deferred[idx]
opt_exec_t = opt_execve[idx]
opt_defer_t = opt_deferred[idx]

# Baseline timeline
ax = axes[0]
ax.barh(0, base_defer_t, left=0, height=0.4, color=RED, alpha=0.7,
        label=f'exit_mmap (sync): {base_defer_t:,.0f} μs')
ax.barh(0, base_exec_t - base_defer_t, left=base_defer_t, height=0.4,
        color=GRAY, alpha=0.5, label=f'Other overhead')
ax.set_title(f'Baseline ({largest_bss} MB process)', fontsize=11, fontweight='bold', pad=15)
ax.set_yticks([])
ax.legend(loc='upper right', bbox_to_anchor=(1, 1.3), ncol=2, fontsize=9)
ax.axvline(x=base_exec_t, color='black', linestyle='--', alpha=0.7)
ax.text(base_exec_t, 0.45, f'execve returns to user\n({base_exec_t:,.0f} μs)',
        ha='right', va='bottom', fontsize=10, fontweight='bold',
        bbox=dict(facecolor='white', alpha=0.9, edgecolor='red', boxstyle='round,pad=0.3'))

# Optimized timeline
ax = axes[1]
ax.barh(0, opt_defer_t, left=0, height=0.4, color=GREEN, alpha=0.9,
        label=f'Phase 1 (sync): {opt_defer_t:,.1f} μs')
ax.barh(0, opt_exec_t - opt_defer_t, left=opt_defer_t, height=0.4,
        color=GRAY, alpha=0.5, label=f'Other overhead')
# Background Phase 2
ax.barh(-0.5, base_defer_t, left=opt_defer_t, height=0.3, color=ORANGE, alpha=0.8,
        label=f'Phase 2 (async background worker): ~{base_defer_t:,.0f} μs')
ax.set_title(f'Optimized ({largest_bss} MB process)', fontsize=11, fontweight='bold', pad=15)
ax.set_yticks([])
ax.set_xlabel('Time elapsed from execve entry (μs)', fontsize=11)
ax.legend(loc='upper right', bbox_to_anchor=(1, 1.35), ncol=2, fontsize=9)
ax.axvline(x=opt_exec_t, color='black', linestyle='--', alpha=0.7)
ax.text(opt_exec_t, 0.45, f'execve returns to user\n({opt_exec_t:,.0f} μs)',
        ha='left', va='bottom', fontsize=10, fontweight='bold',
        bbox=dict(facecolor='white', alpha=0.9, edgecolor='green', boxstyle='round,pad=0.3'))

fig.suptitle('Chronological Timeline: Execution path of a massive process', fontsize=14, y=0.98)
fig.tight_layout()
path4 = os.path.join(output_dir, 'fig4_timeline.png')
fig.savefig(path4)
print(f"Saved: {path4}")
plt.close()

# ══════════════════════════════════════════════════════════════════
# FIGURE 5: Combined Table (for report)
# ══════════════════════════════════════════════════════════════════
fig, ax = plt.subplots(figsize=(10, 4))
ax.axis('off')

table_data = []
headers = ['Process\nSize', 'Baseline\nexecve (μs)', 'Optimized\nexecve (μs)',
           'Speedup', 'Baseline\nsync cost (μs)', 'Optimized\nsync cost (μs)',
           'Sync\nreduction']

for i in range(len(bss_sizes)):
    s = base_execve[i] / opt_execve[i] if opt_execve[i] > 0 else 0
    d = base_deferred[i] / opt_deferred[i] if opt_deferred[i] > 0 else 0
    table_data.append([
        f"{bss_sizes[i]} MB",
        f"{base_execve[i]:,.0f}",
        f"{opt_execve[i]:,.0f}",
        f"{s:.0f}×",
        f"{base_deferred[i]:,.0f}",
        f"{opt_deferred[i]:,.1f}",
        f"{d:.0f}×",
    ])

table = ax.table(cellText=table_data, colLabels=headers,
                 cellLoc='center', loc='center')
table.auto_set_font_size(False)
table.set_fontsize(9)
table.scale(1.0, 1.5)

# Style header
for (row, col), cell in table.get_celld().items():
    if row == 0:
        cell.set_facecolor('#1e3a5f')
        cell.set_text_props(color='white', fontweight='bold')
    elif row % 2 == 0:
        cell.set_facecolor('#f0f4f8')

ax.set_title('Benchmark Results Summary', fontsize=13, pad=20)
fig.tight_layout()
path5 = os.path.join(output_dir, 'fig5_summary_table.png')
fig.savefig(path5)
print(f"Saved: {path5}")
plt.close()

print(f"\nAll figures saved to: {output_dir}/")
print("Files: fig1_execve_comparison.png, fig2_sync_cost.png,")
print("       fig3_speedup.png, fig4_timeline.png, fig5_summary_table.png")
