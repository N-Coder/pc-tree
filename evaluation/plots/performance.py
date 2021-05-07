from itertools import permutations

from scipy import stats

from common import *

df = pd.json_normalize(db.planarity_performance.find())


def get_cols(pre="", suf="", inv=False):
    return [col for col in df.dtypes.index if (col.startswith(pre) and col.endswith(suf)) == (not inv)]


def parse_impl_name(s):
    return s.replace("times.", "")


df_melt = df.melt(["id", "edges", "nodes"], get_cols(pre="times."), var_name="impl", value_name="time")
df_melt["impl"] = df_melt["impl"].apply(parse_impl_name)
df_melt2 = df_melt[2 * df_melt["nodes"] == df_melt["edges"]]
df_melt3 = df_melt[3 * df_melt["nodes"] - 6 == df_melt["edges"]]

for a, b in permutations(get_cols(pre="times."), 2):
    df["%s/%s" % (parse_impl_name(a), parse_impl_name(b))] = df[a] / df[b]

# %%

sns.lineplot(x="nodes", y="time", hue="impl", data=df_melt2)
plt.show()
sns.lineplot(x="nodes", y="time", hue="impl", data=df_melt3)
plt.show()


# %%


def make_boxplots(data, x, y, xlabel, ylabel, max_size, interval_size, ticks_every, filename):
    fig, ax = plt.subplots()
    data = data.dropna(subset=[x, y])
    sns.boxplot(x=pd.cut(data[x], np.arange(0, max_size + interval_size, interval_size)), y=y, data=data,
                showfliers=False, ax=ax)
    tick_range = range(ticks_every - 1, max_size // interval_size, ticks_every)
    ax.set_xticks(tick_range)
    ax.set_xticklabels([interval_size * (i + 1) for i in tick_range])
    ax.set(ylabel=ylabel, xlabel=xlabel)
    # fig.savefig(filename)
    return fig


for a, b in [
    ("BoyerMyrvold::isPlanarDestructive", "BoothLueker::isPlanarDestructive"),
    ("BoothLueker::doTest", "BoothLueker::isPlanarDestructive"),
    ("stNumbering", "BoothLueker::isPlanarDestructive"),
    ("UFPC::isPlanar", "BoothLueker::doTest"),
]:
    label = "Runtime difference %s / %s" % (a, b)
    col = "%s/%s" % (a, b)
    path = col.replace("/", "_")
    print(a, b, len(df[col]), stats.describe(df[col].dropna()))
    fig = make_boxplots(df, "nodes", col, "Number of Nodes", label,
                        1000000, 100000, 2, "planaritytest_speedup_impl_%s.png" % path)
    fig.show()
    plt.close(fig)
