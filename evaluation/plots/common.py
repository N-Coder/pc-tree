import warnings

warnings.simplefilter(action='ignore', category=FutureWarning)

import os
import sys
from collections import defaultdict, Counter
from pprint import pprint

import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
from matplotlib.colors import LogNorm, Normalize
from pymongo import MongoClient, ASCENDING as ASC
from tabulate import tabulate
from tqdm import tqdm

INTERACTIVE = hasattr(sys, 'ps1')


def show_fig(fig, keep=False):
    if INTERACTIVE:
        fig.show()
    if not keep:
        plt.close(fig)


def get_cols(pre="", suf="", cont="", inv=False):
    def func(val):
        if isinstance(val, str):
            return (val.startswith(pre) and val.endswith(suf) and (cont in val)) == (not inv)
        else:
            return filter(func, val)

    return func


def rename_cols(part, replace=""):
    def func(val):
        if isinstance(val, str):
            return val.replace(part, replace)
        else:
            return filter(func, val)

    return func


def melt(df, id_vars=None, value_vars=None, var_name=None, value_name="value",
         map_var=None, **kwargs):
    if callable(id_vars):
        id_vars = id_vars(df.dtypes.index)
    if callable(value_vars):
        value_vars = value_vars(df.dtypes.index)
    id_vars = list(id_vars)
    df = df.melt(id_vars, value_vars, var_name, value_name, **kwargs)
    if map_var:
        for c in df.dtypes.index:
            if c != value_name and c not in id_vars:
                df[c] = df[c].apply(map_var)
    return df


def make_speedupplot(data, x, ylim, range, filename, loc="upper right"):
    fig, ax = plt.subplots()

    ys = ["speedup.%s" % impl for impl in IMPLS_COMPARE]
    data = data.dropna(subset=[x, *ys])
    x_buckets = pd.cut(data[x], range)
    groups = data.groupby(x_buckets)

    # for impl in IMPLS_COMPARE:
    #     sns.scatterplot(x=x, y="speedup.%s" % impl, label=impl, data=data, ax=ax, s=25, alpha=0.2,
    #                     color=COLORS[impl], marker=MARKERS[impl])

    for impl in IMPLS_COMPARE:
        y = "speedup.%s" % impl
        lower = groups[y].quantile(0.25).reset_index()
        upper = groups[y].quantile(0.75).reset_index()
        ax.fill_between(lower[x].apply(lambda x: x.mid), lower[y], upper[y], color=COLORS[impl], alpha=0.3)

    for impl in IMPLS_COMPARE:
        y = "speedup.%s" % impl
        line = groups[y].median().reset_index()
        ax.plot(line[x].apply(lambda x: x.mid), line[y], color="white", linewidth=2)
        ax.plot(line[x].apply(lambda x: x.mid), line[y], color=COLORS[impl], linewidth=1, label=impl)

    ax.set_ylabel("Speedup over OGDF")
    ax.set_xlabel(NAMES[x])
    ax.set_ylim(*ylim)
    ax.legend(loc=loc)
    fig.savefig(filename)
    return fig


mpl.rc("figure", figsize=(6.4, 4.8), dpi=90 if INTERACTIVE else 300, autolayout=True)


IMPLS_ASC_FASTEST = ["UFPC", "HsuPC", "CppZanetti", "Zanetti", "OGDF"]
IMPLS_DESC_FASTEST = list(reversed(IMPLS_ASC_FASTEST))
IMPLS_ASC = IMPLS_ASC_FASTEST + ["GraphSet", "Gregable", "BiVoc", "Reisle", "JGraphEd", "SageMath"]
IMPLS_DESC = list(reversed(IMPLS_ASC))
IMPLS_COMPARE = ["UFPC/OGDF", "HsuPC/OGDF", "Zanetti/OGDF", "CppZanetti/OGDF"]
COLOR_PALETTE = sns.color_palette("bright", n_colors=len(IMPLS_ASC) + 1)
COLORS = dict(zip(IMPLS_ASC, COLOR_PALETTE))
COLORS["SageMath"] = (0.2, 0.2, 0.2)
COLORS = {**COLORS, **{f"%s/OGDF" % impl: COLORS[impl] for impl in IMPLS_ASC}}
MARKERS_ALL = ['H', 'D', 'X', 'o', 'P', 'v', '<', '>', '^', 'p', 'h']
MARKERS = dict(zip(IMPLS_ASC, MARKERS_ALL))
NAMES = {
    "tree_size": "Tree Size",
    "leaves": "Leaves",
    "tp_length": "Terminal Path Length",
    "full_leaves": "Restriction Size",
    "time": "Time [ns]",
    "cleanup_time": "Cleanup Time [ns]",
    "total_time": "Total Time [ns]",
}
HEATMAP = sns.color_palette("rocket_r", as_cmap=True).copy()
HEATMAP.set_under("white")
HEATMAP.set_over("white")
HEATMAP.set_bad("white")

db = pymongo.MongoClient(
    "pc-tree-mongo", connectTimeoutMS=2000 
).pc_tree

os.makedirs("out", exist_ok=True)
os.chdir("out")
