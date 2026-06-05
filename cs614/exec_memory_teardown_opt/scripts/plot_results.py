#!/usr/bin/env python3
"""
plot_results.py — Parse async/sync test results and generate comparison plots.

Usage:
    python3 plot_results.py [results_dir]

Outputs 10 visualizations + text summary.
"""

import os
import re
import sys
import statistics

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.patches import FancyBboxPatch
    import numpy as np
    HAS_MPL = True
except ImportError:
    HAS_MPL = False
    print("WARNING: matplotlib not installed. Install with:")
    print("  pip install matplotlib --break-system-packages")

# test2 is now 100 MB (fixed from duplicate 200 MB)
TESTS = ['test0', 'test1', 'test2', 'test3', 'test4', 'test5',
         'test6', 'test7', 'test8', 'test9']
MEMORY_MB = [0, 50, 100, 200, 400, 600, 800, 1024, 1600, 2048]
MEMORY_LABELS = ['~0', '50', '100', '200', '400', '600', '800', '1024', '1600', '2048']

ASYNC_COLOR = '#10b981'
SYNC_COLOR = '#ef4444'
SPEEDUP_COLOR = '#f59e0b'

def parse_async(filepath):
    times = []
    with open(filepath, 'r') as f:
        for line in f:
            m = re.search(r'elapsed=(\d+)\s*ns', line)
            if m:
                times.append(int(m.group(1)))
    return times

def parse_sync(filepath):
    times = []
    with open(filepath, 'r') as f:
        for line in f:
            m = re.search(r'real\s+(\d+)m([\d.]+)s', line)
            if m:
                times.append(int(m.group(1)) * 60 + float(m.group(2)))
    return times

def make_summary(async_data, sync_data):
    rows = []
    for i, test in enumerate(TESTS):
        a_times = async_data.get(test, [])
        s_times = sync_data.get(test, [])
        if not a_times or not s_times:
            continue
        if len(s_times) > 2 and s_times[0] > s_times[1] * 3:
            s_times = s_times[1:]
        a_us = [t / 1000 for t in a_times]
        s_ms = [t * 1000 for t in s_times]
        rows.append({
            'test': test, 'memory': MEMORY_LABELS[i], 'mem_mb': MEMORY_MB[i],
            'async_us_list': a_us, 'sync_ms_list': s_ms,
            'async_median_us': statistics.median(a_us),
            'async_avg_us': statistics.mean(a_us),
            'async_p25_us': statistics.quantiles(a_us, n=4)[0] if len(a_us) >= 4 else min(a_us),
            'async_p75_us': statistics.quantiles(a_us, n=4)[2] if len(a_us) >= 4 else max(a_us),
            'sync_median_ms': statistics.median(s_ms),
            'sync_avg_ms': statistics.mean(s_ms),
            'sync_p25_ms': statistics.quantiles(s_ms, n=4)[0] if len(s_ms) >= 4 else min(s_ms),
            'sync_p75_ms': statistics.quantiles(s_ms, n=4)[2] if len(s_ms) >= 4 else max(s_ms),
            'speedup': (statistics.median(s_ms) * 1000) / statistics.median(a_us),
            'n': min(len(a_us), len(s_ms)),
        })
    return rows


def plot_comparison(rows, out_path):
    fig, ax = plt.subplots(figsize=(13, 6))
    labels = [r['memory'] for r in rows]
    async_ms = [r['async_median_us'] / 1000 for r in rows]
    sync_ms = [r['sync_median_ms'] for r in rows]
    x = np.arange(len(labels))
    width = 0.38
    ax.bar(x - width/2, sync_ms, width, label='Sync (module OFF)',
           color=SYNC_COLOR, alpha=0.85, edgecolor='#b91c1c', linewidth=0.5)
    ax.bar(x + width/2, async_ms, width, label='Async exec (module ON)',
           color=ASYNC_COLOR, alpha=0.85, edgecolor='#047857', linewidth=0.5)
    ax.set_yscale('log')
    ax.set_xlabel('Memory Footprint (MB)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ms, log scale)', fontsize=12, fontweight='bold')
    ax.set_title('Exec Time Comparison: Sync vs Async (50 runs, median)',
                 fontsize=14, fontweight='bold', pad=15)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend(fontsize=11, loc='upper left')
    ax.grid(axis='y', alpha=0.3, which='both')
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_speedup(rows, out_path):
    fig, ax = plt.subplots(figsize=(13, 5))
    labels = [r['memory'] for r in rows]
    speedups = [r['speedup'] for r in rows]
    x = np.arange(len(labels))
    colors = [SPEEDUP_COLOR if s < 1000 else '#ea580c' for s in speedups]
    bars = ax.bar(x, speedups, color=colors, alpha=0.9, edgecolor='#c2410c', linewidth=0.6)
    for bar, s in zip(bars, speedups):
        ax.text(bar.get_x() + bar.get_width()/2., bar.get_height() + max(speedups) * 0.01,
                f'{s:.0f}×', ha='center', va='bottom', fontsize=10, fontweight='bold')
    ax.set_xlabel('Memory Footprint (MB)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Speedup (Sync ÷ Async)', fontsize=12, fontweight='bold')
    ax.set_title('Speedup Factor: How Much Faster is Async Teardown',
                 fontsize=14, fontweight='bold', pad=15)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.grid(axis='y', alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_async_flatline(rows, out_path):
    fig, ax = plt.subplots(figsize=(13, 5))
    labels = [r['memory'] for r in rows]
    async_us = [r['async_median_us'] for r in rows]
    x = np.arange(len(labels))
    bars = ax.bar(x, async_us, color=ASYNC_COLOR, alpha=0.85, edgecolor='#047857', linewidth=0.6)
    for bar, v in zip(bars, async_us):
        ax.text(bar.get_x() + bar.get_width()/2., bar.get_height() + max(async_us) * 0.015,
                f'{v:.0f}', ha='center', va='bottom', fontsize=9)
    median = statistics.median(async_us)
    ax.axhline(y=median, color='#1f2937', linestyle='--', linewidth=1, alpha=0.5,
               label=f'Overall median: {median:.0f} µs')
    ax.set_xlabel('Memory Footprint (MB)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Async Exec Time (µs)', fontsize=12, fontweight='bold')
    ax.set_title('Async Exec Latency Stays Flat Regardless of Memory Size',
                 fontsize=13, fontweight='bold', pad=15)
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend(fontsize=10)
    ax.grid(axis='y', alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_boxplots(rows, out_path):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    labels = [r['memory'] for r in rows]
    async_data = [r['async_us_list'] for r in rows]
    sync_data = [r['sync_ms_list'] for r in rows]

    bp1 = ax1.boxplot(async_data, labels=labels, patch_artist=True, showfliers=True,
                      widths=0.55,
                      flierprops=dict(marker='o', markerfacecolor='#fb7185', markersize=4, alpha=0.6))
    for patch in bp1['boxes']:
        patch.set_facecolor(ASYNC_COLOR)
        patch.set_alpha(0.6)
    for median in bp1['medians']:
        median.set_color('#064e3b')
        median.set_linewidth(2)
    ax1.set_yscale('log')
    ax1.set_title('Async Exec Time Distribution (50 runs)', fontsize=13, fontweight='bold')
    ax1.set_xlabel('Memory (MB)', fontsize=11, fontweight='bold')
    ax1.set_ylabel('Time (µs, log scale)', fontsize=11, fontweight='bold')
    ax1.grid(axis='y', alpha=0.3, which='both')

    bp2 = ax2.boxplot(sync_data, labels=labels, patch_artist=True, showfliers=True,
                      widths=0.55,
                      flierprops=dict(marker='o', markerfacecolor='#7f1d1d', markersize=4, alpha=0.6))
    for patch in bp2['boxes']:
        patch.set_facecolor(SYNC_COLOR)
        patch.set_alpha(0.6)
    for median in bp2['medians']:
        median.set_color('#7f1d1d')
        median.set_linewidth(2)
    ax2.set_title('Sync Total Time Distribution (50 runs)', fontsize=13, fontweight='bold')
    ax2.set_xlabel('Memory (MB)', fontsize=11, fontweight='bold')
    ax2.set_ylabel('Time (ms)', fontsize=11, fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)

    fig.suptitle('Run Distribution: Async exec is tightly clustered, Sync grows with memory',
                 fontsize=14, fontweight='bold', y=1.02)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_violins(rows, out_path):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    labels = [r['memory'] for r in rows]
    async_data = [r['async_us_list'] for r in rows]
    sync_data = [r['sync_ms_list'] for r in rows]
    positions = list(range(1, len(rows) + 1))

    parts1 = ax1.violinplot(async_data, positions=positions, widths=0.7,
                            showmeans=False, showmedians=True, showextrema=True)
    for pc in parts1['bodies']:
        pc.set_facecolor(ASYNC_COLOR)
        pc.set_alpha(0.6)
        pc.set_edgecolor('#047857')
    ax1.set_yscale('log')
    ax1.set_xticks(positions)
    ax1.set_xticklabels(labels)
    ax1.set_title('Async Exec — Run Density (violin)', fontsize=13, fontweight='bold')
    ax1.set_xlabel('Memory (MB)', fontsize=11, fontweight='bold')
    ax1.set_ylabel('Time (µs, log scale)', fontsize=11, fontweight='bold')
    ax1.grid(axis='y', alpha=0.3, which='both')

    parts2 = ax2.violinplot(sync_data, positions=positions, widths=0.7,
                            showmeans=False, showmedians=True, showextrema=True)
    for pc in parts2['bodies']:
        pc.set_facecolor(SYNC_COLOR)
        pc.set_alpha(0.6)
        pc.set_edgecolor('#b91c1c')
    ax2.set_xticks(positions)
    ax2.set_xticklabels(labels)
    ax2.set_title('Sync Total — Run Density (violin)', fontsize=13, fontweight='bold')
    ax2.set_xlabel('Memory (MB)', fontsize=11, fontweight='bold')
    ax2.set_ylabel('Time (ms)', fontsize=11, fontweight='bold')
    ax2.grid(axis='y', alpha=0.3)

    fig.suptitle('Distribution Density of Run Times Across 50 Iterations',
                 fontsize=14, fontweight='bold', y=1.02)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_line_with_bands(rows, out_path):
    fig, ax = plt.subplots(figsize=(13, 6))
    mem_mb = [r['mem_mb'] if r['mem_mb'] > 0 else 1 for r in rows]
    async_med = [r['async_median_us'] / 1000 for r in rows]
    async_p25 = [r['async_p25_us'] / 1000 for r in rows]
    async_p75 = [r['async_p75_us'] / 1000 for r in rows]
    sync_med = [r['sync_median_ms'] for r in rows]
    sync_p25 = [r['sync_p25_ms'] for r in rows]
    sync_p75 = [r['sync_p75_ms'] for r in rows]

    ax.fill_between(mem_mb, sync_p25, sync_p75, color=SYNC_COLOR, alpha=0.2,
                    label='Sync 25th–75th percentile')
    ax.plot(mem_mb, sync_med, color=SYNC_COLOR, linewidth=2.5, marker='s',
            markersize=8, label='Sync median')
    ax.fill_between(mem_mb, async_p25, async_p75, color=ASYNC_COLOR, alpha=0.2,
                    label='Async 25th–75th percentile')
    ax.plot(mem_mb, async_med, color=ASYNC_COLOR, linewidth=2.5, marker='o',
            markersize=8, label='Async median')

    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlabel('Memory Footprint (MB, log scale)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ms, log scale)', fontsize=12, fontweight='bold')
    ax.set_title('Latency Trends with Memory: Async stays flat, Sync grows linearly',
                 fontsize=14, fontweight='bold', pad=15)
    ax.legend(fontsize=10, loc='upper left')
    ax.grid(True, alpha=0.3, which='both')
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_scatter(rows, out_path):
    fig, ax = plt.subplots(figsize=(13, 6))
    for i, r in enumerate(rows):
        x_async = [i] * len(r['async_us_list'])
        x_sync = [i] * len(r['sync_ms_list'])
        async_ms = [v / 1000 for v in r['async_us_list']]
        ax.scatter(x_async, async_ms, alpha=0.5, color=ASYNC_COLOR, s=18, zorder=2,
                   label='Async exec' if i == 0 else None)
        ax.scatter(x_sync, r['sync_ms_list'], alpha=0.5, color=SYNC_COLOR, s=18, zorder=2,
                   label='Sync total' if i == 0 else None)

    ax.set_yscale('log')
    ax.set_xticks(range(len(rows)))
    ax.set_xticklabels([r['memory'] for r in rows])
    ax.set_xlabel('Memory Footprint (MB)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ms, log scale)', fontsize=12, fontweight='bold')
    ax.set_title('Every Single Run (50 per test) — Sync vs Async',
                 fontsize=14, fontweight='bold', pad=15)
    ax.legend(fontsize=10, loc='upper left')
    ax.grid(True, alpha=0.3, which='both')
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_heatmap(rows, out_path):
    fig, ax = plt.subplots(figsize=(13, 4))
    speedups = np.array([[r['speedup'] for r in rows]])
    cmap = plt.get_cmap('YlOrRd')
    im = ax.imshow(speedups, cmap=cmap, aspect='auto', vmin=0, vmax=max(speedups[0]))
    for i, r in enumerate(rows):
        text_color = 'white' if r['speedup'] > max(speedups[0]) * 0.5 else 'black'
        ax.text(i, 0, f"{r['speedup']:.0f}×\n{r['memory']} MB",
                ha='center', va='center', fontsize=11, fontweight='bold', color=text_color)
    ax.set_xticks(range(len(rows)))
    ax.set_xticklabels([r['test'] for r in rows])
    ax.set_yticks([0])
    ax.set_yticklabels(['Speedup'])
    ax.set_title('Speedup Heatmap: How Async Performance Scales with Memory',
                 fontsize=14, fontweight='bold', pad=15)
    cbar = plt.colorbar(im, ax=ax, orientation='horizontal', pad=0.15, shrink=0.6)
    cbar.set_label('Speedup factor (×)', fontsize=10)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_timing_breakdown(rows, out_path):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    labels = [r['memory'] for r in rows]
    x = np.arange(len(labels))
    sync_total = [r['sync_median_ms'] for r in rows]
    async_exec = [r['async_median_us'] / 1000 for r in rows]

    ax1.bar(x, sync_total, color=SYNC_COLOR, alpha=0.85, edgecolor='#b91c1c',
            linewidth=0.5, label='Process blocks here')
    ax1.set_title('Sync Mode: Process waits for full teardown',
                  fontsize=13, fontweight='bold')
    ax1.set_xlabel('Memory (MB)', fontsize=11, fontweight='bold')
    ax1.set_ylabel('Wall-clock blocking time (ms)', fontsize=11, fontweight='bold')
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels)
    ax1.legend(fontsize=10)
    ax1.grid(axis='y', alpha=0.3)

    ax2.bar(x, sync_total, color='#9ca3af', alpha=0.4, edgecolor='#6b7280',
            linewidth=0.5, label='Background (kthread, off critical path)')
    ax2.bar(x, async_exec, color=ASYNC_COLOR, alpha=0.95, edgecolor='#047857',
            linewidth=0.5, label='Process blocks here (async exec)')
    ax2.set_title('Async Mode: Process only waits for exec; teardown runs in background',
                  fontsize=13, fontweight='bold')
    ax2.set_xlabel('Memory (MB)', fontsize=11, fontweight='bold')
    ax2.set_ylabel('Time (ms)', fontsize=11, fontweight='bold')
    ax2.set_xticks(x)
    ax2.set_xticklabels(labels)
    ax2.legend(fontsize=10)
    ax2.grid(axis='y', alpha=0.3)

    fig.suptitle('Timing Breakdown: Critical Path vs Background Work',
                 fontsize=14, fontweight='bold', y=1.02)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def plot_architecture(out_path):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7))
    for ax in (ax1, ax2):
        ax.set_xlim(0, 10)
        ax.set_ylim(0, 10)
        ax.axis('off')

    # SYNC
    ax1.set_title('SYNC: Process Blocks on Full Teardown',
                  fontsize=14, fontweight='bold', color='#b91c1c', pad=10)
    boxes_sync = [
        (0.5, 8, 9, 1.2, '#dbeafe', '#1e40af', 2, 'User Process calls execv()', 12, True),
        (1, 6.2, 8, 1, '#fee2e2', '#b91c1c', 1.5, 'kernel: mmput(old_mm)', 11, False),
        (1, 4.4, 8, 1, '#fecaca', '#b91c1c', 1.5, 'exit_mmap() — UNMAPS ALL PAGES (slow!)', 11, True),
        (1, 2.6, 8, 1, '#fee2e2', '#b91c1c', 1.5, 'mmdrop() → free mm_struct', 11, False),
        (0.5, 0.6, 9, 1.2, '#dbeafe', '#1e40af', 2,
         'execv() RETURNS\n(blocked for: page count × time/page)', 11, True),
    ]
    for x, y, w, h, fc, ec, lw, text, fs, bold in boxes_sync:
        box = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.1",
                             facecolor=fc, edgecolor=ec, linewidth=lw)
        ax1.add_patch(box)
        ax1.text(x + w/2, y + h/2, text, ha='center', va='center',
                 fontsize=fs, fontweight='bold' if bold else 'normal')

    for y_start, y_end in [(8, 7.2), (6.2, 5.4), (4.4, 3.6), (2.6, 1.8)]:
        ax1.annotate('', xy=(5, y_end), xytext=(5, y_start),
                     arrowprops=dict(arrowstyle='->', color='#1f2937', lw=1.8))

    ax1.text(9.5, 4.9, 'Process\nWAITS\nhere', ha='center', va='center',
             fontsize=11, fontweight='bold', color='#b91c1c',
             bbox=dict(boxstyle='round,pad=0.3', facecolor='#fef3c7', edgecolor='#d97706'))

    # ASYNC
    ax2.set_title('ASYNC: Process Returns Immediately; Background Cleanup',
                  fontsize=14, fontweight='bold', color='#047857', pad=10)

    box1 = FancyBboxPatch((0.5, 8), 9, 1.2, boxstyle="round,pad=0.1",
                          facecolor='#dbeafe', edgecolor='#1e40af', linewidth=2)
    ax2.add_patch(box1)
    ax2.text(5, 8.6, 'User Process calls execv()', ha='center', va='center',
             fontsize=12, fontweight='bold')

    box2 = FancyBboxPatch((1, 6.2), 8, 1, boxstyle="round,pad=0.1",
                          facecolor='#d1fae5', edgecolor='#047857', linewidth=1.5)
    ax2.add_patch(box2)
    ax2.text(5, 6.7, 'kprobe intercepts mmput() → bumps mm_users 1→2',
             ha='center', va='center', fontsize=10)

    box3a = FancyBboxPatch((0.3, 3.5), 4.5, 2.3, boxstyle="round,pad=0.1",
                           facecolor='#dcfce7', edgecolor='#047857', linewidth=2)
    ax2.add_patch(box3a)
    ax2.text(2.55, 5.3, 'Process (foreground)', ha='center', va='center',
             fontsize=11, fontweight='bold', color='#047857')
    ax2.text(2.55, 4.6, '✓ Skip exit_mmap\n✓ Continue with new ELF\n✓ ~150 µs total',
             ha='center', va='center', fontsize=10)

    box3b = FancyBboxPatch((5.2, 3.5), 4.5, 2.3, boxstyle="round,pad=0.1",
                           facecolor='#fef3c7', edgecolor='#d97706', linewidth=2)
    ax2.add_patch(box3b)
    ax2.text(7.45, 5.3, 'mm_destroyer kthread (background)',
             ha='center', va='center', fontsize=11, fontweight='bold', color='#d97706')
    ax2.text(7.45, 4.6, '• exit_mmap()\n• mmdrop()\n• Off the critical path',
             ha='center', va='center', fontsize=10)

    box5 = FancyBboxPatch((0.3, 0.6), 4.5, 1.2, boxstyle="round,pad=0.1",
                          facecolor='#dbeafe', edgecolor='#1e40af', linewidth=2)
    ax2.add_patch(box5)
    ax2.text(2.55, 1.2, 'execv() RETURNS\n(in ~150 µs always)',
             ha='center', va='center', fontsize=11, fontweight='bold')

    box6 = FancyBboxPatch((5.2, 0.6), 4.5, 1.2, boxstyle="round,pad=0.1",
                          facecolor='#fef9c3', edgecolor='#d97706', linewidth=2)
    ax2.add_patch(box6)
    ax2.text(7.45, 1.2, 'Pages freed in background\n(takes 100–500 ms)',
             ha='center', va='center', fontsize=11, fontweight='bold')

    ax2.annotate('', xy=(5, 7.2), xytext=(5, 8), arrowprops=dict(arrowstyle='->', color='#1f2937', lw=1.8))
    ax2.annotate('', xy=(2.55, 5.8), xytext=(4, 6.2), arrowprops=dict(arrowstyle='->', color='#047857', lw=1.8))
    ax2.annotate('', xy=(7.45, 5.8), xytext=(6, 6.2), arrowprops=dict(arrowstyle='->', color='#d97706', lw=1.8))
    ax2.annotate('', xy=(2.55, 1.8), xytext=(2.55, 3.5), arrowprops=dict(arrowstyle='->', color='#1f2937', lw=1.8))
    ax2.annotate('', xy=(7.45, 1.8), xytext=(7.45, 3.5), arrowprops=dict(arrowstyle='->', color='#1f2937', lw=1.8))

    fig.suptitle('Architecture: How Async MM Teardown Works',
                 fontsize=15, fontweight='bold', y=1.0)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150, bbox_inches='tight')
    plt.close(fig)


def main():
    if len(sys.argv) > 1:
        results_dir = sys.argv[1]
    else:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        results_dir = os.path.join(os.path.dirname(script_dir), 'results')

    if not os.path.isdir(results_dir):
        print(f"ERROR: Results directory not found: {results_dir}")
        sys.exit(1)

    async_data = {}
    sync_data = {}
    for test in TESTS:
        af = os.path.join(results_dir, f'async_avg_{test}.txt')
        sf = os.path.join(results_dir, f'sync_avg_{test}.txt')
        if os.path.exists(af):
            async_data[test] = parse_async(af)
        if os.path.exists(sf):
            sync_data[test] = parse_sync(sf)

    rows = make_summary(async_data, sync_data)

    print("=" * 100)
    print(f"{'Test':<8} {'Memory':<10} {'Async Median':>14} {'Async Avg':>14} "
          f"{'Sync Median':>14} {'Sync Avg':>12} {'Speedup':>10} {'N':>5}")
    print("=" * 100)
    for r in rows:
        print(f"{r['test']:<8} {r['memory'] + ' MB':<10} "
              f"{r['async_median_us']:>11.1f} µs {r['async_avg_us']:>11.1f} µs "
              f"{r['sync_median_ms']:>11.1f} ms {r['sync_avg_ms']:>9.1f} ms "
              f"{r['speedup']:>9.0f}× {r['n']:>5}")
    print("=" * 100)

    with open(os.path.join(results_dir, 'summary_table.txt'), 'w') as f:
        f.write(f"{'Test':<8} {'Memory':<10} {'Async Median (µs)':>20} "
                f"{'Sync Median (ms)':>20} {'Speedup':>10}\n")
        f.write("-" * 70 + "\n")
        for r in rows:
            f.write(f"{r['test']:<8} {r['memory'] + ' MB':<10} "
                    f"{r['async_median_us']:>20.1f} "
                    f"{r['sync_median_ms']:>20.1f} "
                    f"{r['speedup']:>9.0f}×\n")
    print(f"\nText summary saved to: {os.path.join(results_dir, 'summary_table.txt')}")

    if not HAS_MPL:
        return

    print("\nGenerating 10 visualizations...")
    plot_comparison(rows, os.path.join(results_dir, '01_comparison_chart.png'))
    print("  ✓ 01_comparison_chart.png")
    plot_speedup(rows, os.path.join(results_dir, '02_speedup_chart.png'))
    print("  ✓ 02_speedup_chart.png")
    plot_async_flatline(rows, os.path.join(results_dir, '03_async_flatline_chart.png'))
    print("  ✓ 03_async_flatline_chart.png")
    plot_boxplots(rows, os.path.join(results_dir, '04_boxplot_distribution.png'))
    print("  ✓ 04_boxplot_distribution.png")
    plot_violins(rows, os.path.join(results_dir, '05_violin_distribution.png'))
    print("  ✓ 05_violin_distribution.png")
    plot_line_with_bands(rows, os.path.join(results_dir, '06_line_with_bands.png'))
    print("  ✓ 06_line_with_bands.png")
    plot_scatter(rows, os.path.join(results_dir, '07_scatter_all_runs.png'))
    print("  ✓ 07_scatter_all_runs.png")
    plot_heatmap(rows, os.path.join(results_dir, '08_speedup_heatmap.png'))
    print("  ✓ 08_speedup_heatmap.png")
    plot_timing_breakdown(rows, os.path.join(results_dir, '09_timing_breakdown.png'))
    print("  ✓ 09_timing_breakdown.png")
    plot_architecture(os.path.join(results_dir, '10_architecture_diagram.png'))
    print("  ✓ 10_architecture_diagram.png")
    print("\nAll plots generated successfully!")

if __name__ == '__main__':
    main()
