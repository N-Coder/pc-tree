from itertools import permutations

from scipy import stats

from common import *

NAMES["size"] = NAMES["full_leaves"]


# %%

def load_big_planarity_groups():
    merged_doc = {}
    for doc in tqdm(db.planarity_results.find(sort=[("file", ASC), ("index", ASC)], projection={"id": 0})):
        if not merged_doc or merged_doc["file"] != doc["file"] or merged_doc["index"] != doc["index"]:
            if merged_doc:
                yield merged_doc
            merged_doc = {
                "file": doc["file"],
                "index": doc["index"],
                "entries": {},
            }
        if "tp_length" in doc:
            merged_doc["tp_length"] = doc["tp_length"]
        if "size" in doc:
            merged_doc["size"] = doc["size"]  # TODO rename to full_leaves
        assert doc["type"] not in merged_doc["entries"]
        merged_doc["entries"][doc["type"]] = doc
    yield merged_doc


def filter_groups(groups, errors):
    for group in groups:
        if "UFPC" not in group["entries"]:
            group["errors"] = [(k, "no UFPC") for k in group["entries"]]
            errors.append(group)
            continue
        ufpc = group["entries"]["UFPC"]
        group["errors"] = []
        for t, e in list(group["entries"].items()):
            if ufpc["size"] != e.get("size", ""):
                group["errors"].append((t, "size != UFPC"))
            if ufpc["result"] != e.get("result", ""):
                group["errors"].append((t, "result != UFPC"))
            if ufpc.get("fingerprint", "") != e.get("fingerprint", ""):
                group["errors"].append((t, "fingerprint != UFPC"))
            if e.get("exception", ""):
                group["errors"].append((t, "exception"))
            if t == "CppZanetti":
                if "Zanetti" in group["entries"]:
                    if group["entries"]["Zanetti"].get("tree", "") != e.get("tree", ""):
                        group["errors"].append((t, "tree != Zanetti"))
                else:
                    group["errors"].append((t, "no Zanetti"))
            elif t != "OGDF":
                if ufpc.get("uid", "") != e.get("uid", ""):
                    group["errors"].append((t, "uid != UFPC"))
        if group["errors"]:
            errors.append({**group, "entries": dict(group["entries"])})
            for t, err in group["errors"]:
                group["entries"].pop(t, None)
        else:
            del group["errors"]
        if group["entries"]:
            group["speedup"] = {
                "%s/%s" % (a, b): group["entries"][a]["time"] / group["entries"][b]["time"]
                for a, b in permutations(group["entries"], 2)
            }
            yield group


errors = []
df = pd.json_normalize(filter_groups(load_big_planarity_groups(), errors))
df = df[df["size"] >= 25]
pprint(Counter(frozenset(e["errors"]) for e in errors))

# %%

for t in IMPLS_DESC_FASTEST:
    col = df["entries.%s.time" % t]
    print(t, len(col), stats.describe(col.dropna()), col.median())


# %%

def overlay_lines(ax, data, x, y, color, range):
    data = data.dropna(subset=[x, y])
    x_buckets = pd.cut(data[x], range)
    data = data[y].groupby(x_buckets).median().reset_index()
    data[x] = data[x].apply(lambda x: x.mid)
    sns.lineplot(
        x=x, y=y,
        dashes=False, markers=False, linewidth=2, color="white",
        data=data, legend=False, ax=ax
    )
    sns.lineplot(
        x=x, y=y,
        dashes=False, markers=False, linewidth=1, color=color,
        data=data, legend=False, ax=ax
    )


def make_scatterplot(data, x, ylim, range, filename):
    fig, ax = plt.subplots()
    for impl in IMPLS_DESC_FASTEST:
        sns.scatterplot(x=x, y="entries.%s.time" % impl, label=impl, data=data, ax=ax, s=25, alpha=0.5,
                        color=COLORS[impl], marker=MARKERS[impl])
    for impl in IMPLS_DESC_FASTEST:
        overlay_lines(ax, data, x, "entries.%s.time" % impl, COLORS[impl], range)
    ax.set(ylabel="Time [ns]", xlabel=NAMES[x])
    ax.set_ylim(*ylim)
    ax.legend(loc='upper right')
    fig.savefig(filename)
    return fig


# %%

fig = make_scatterplot(
    df[df["file"].str.contains("graphs2n")],
    "size", (-150000, 2000000), np.arange(0, 1500, 100),
    "planaritytest_scatter_size_2n.png")
# show_fig(fig, keep=True)
# fig.axes[0].set_ylim(0, 50000)
# fig.axes[0].set_xlim(25, 250)
# fig.savefig("planaritytest_scatter_size_2n_zoom.png")
show_fig(fig)

fig = make_scatterplot(
    df[df["file"].str.contains("graphs3n")],
    "size", (-150000, 2000000), np.arange(0, 3500, 250),
    "planaritytest_scatter_size_3n.png")
# show_fig(fig, keep=True)
# fig.axes[0].set_ylim(0, 50000)
# fig.axes[0].set_xlim(25, 250)
# fig.savefig("planaritytest_scatter_size_3n_zoom.png")
show_fig(fig)

# %%

fig = make_scatterplot(
    df[df["file"].str.contains("graphs2n")],
    "tp_length", (-150000, 2000000), np.arange(0, 45, 5),
    "planaritytest_scatter_tp_length_2n.png")
show_fig(fig)
fig = make_scatterplot(
    df[df["file"].str.contains("graphs3n")],
    "tp_length", (-150000, 2000000), np.arange(0, 66, 6),
    "planaritytest_scatter_tp_length_3n.png")
show_fig(fig)

# %%

# def make_boxplots(data, x, y, xlabel, ylabel, max_size, interval_size, ticks_every, color, filename):
#     fig, ax = plt.subplots()
#     data = data.dropna(subset=[x, y])
#     sns.boxplot(x=pd.cut(data[x], np.arange(0, max_size + interval_size, interval_size)), y=y, data=data,
#                 showfliers=False, ax=ax, color=color)
#     tick_range = range(ticks_every - 1, max_size // interval_size, ticks_every)
#     ax.set_xticks(tick_range)
#     ax.set_xticklabels([interval_size * (i + 1) for i in tick_range])
#     ax.set(ylabel=ylabel, xlabel=xlabel)
#     fig.savefig(filename)
#     return fig
#
#
# for pair in ["UFPC/OGDF", "HsuPC/OGDF", "Zanetti/OGDF", "CppZanetti/OGDF",
#              "UFPC/HsuPC", "UFPC/CppZanetti", "CppZanetti/Zanetti"]:
#     col = "speedup.%s" % pair
#     label = "Runtime difference " + pair.replace("/", " / ")
#     print(pair, len(df[col]), stats.describe(df[col].dropna()))
#     color = COLORS[pair.split("/")[0]]
#     path = pair.replace("/", "_")
#     fig = make_boxplots(df, "size", col, "Restriction Size", label,
#                         3000, 100, 5, color, "planaritytest_speedup_size_%s.png" % path)
#     plt.close(fig)
#     fig = make_boxplots(df, "tp_length", col, "Terminal Path Length", label,
#                         60, 3, 2, color, "planaritytest_speedup_tp_length_%s.png" % path)
#     plt.close(fig)


# %%


# df_speedup = melt(df, get_cols(cont=".", inv=True), get_cols(pre="speedup."), map_var=rename_cols("speedup."))
# df = df.sample(frac=1, random_state=4242)  # shuffle
# df["tree_type"] = pd.Categorical(df["tree_type"], categories=IMPLS_DESC, ordered=True)
# df.sort_values("tree_type", inplace=True)

MARKERS["UFPC/OGDF"] = MARKERS["UFPC"]
MARKERS["HsuPC/OGDF"] = MARKERS["HsuPC"]
MARKERS["Zanetti/OGDF"] = MARKERS["Zanetti"]
MARKERS["CppZanetti/OGDF"] = MARKERS["CppZanetti"]


# %%

fig = make_speedupplot(
    df,
    "size", (0, 1), np.concatenate((np.arange(0, 1000, 100), np.arange(1000, 3500, 250))),
    "planaritytest_speedup_size.png")
fig.show()
fig = make_speedupplot(
    df,
    "tp_length", (0, 1), np.arange(0, 66, 6),
    "planaritytest_speedup_tp_length.png", loc="lower left")
fig.show()


# %%


def distplot_instances(df, x_vals="size", y_vals="tp_length"):
    BUCKETS = {
        "size": np.unique(np.rint(np.logspace(np.log2(25), np.log2(4500), base=2)).astype(int)),
        "tp_length": np.linspace(0, 64, 32, dtype=int),
    }
    x_buckets = pd.cut(df[x_vals], BUCKETS[x_vals])
    y_buckets = pd.cut(df[y_vals], BUCKETS[y_vals])
    counts = df.groupby([y_buckets, x_buckets]).size().unstack()
    fig, ax = plt.subplots()
    sns.heatmap(data=counts, cbar=True, cmap=HEATMAP, ax=ax,  # vmin=1,
                norm=LogNorm(vmin=1, vmax=counts.max().max()))
    ax.invert_yaxis()
    ax.set(xlabel=NAMES[x_vals], ylabel=NAMES[y_vals])
    fig.savefig("planaritytest_distplot_%s_%s.png" % (x_vals, y_vals))
    return fig


print("distplot")
fig = distplot_instances(df)
show_fig(fig)
