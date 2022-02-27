from common import *

IMPOSSIBLE_SUFFIX = {False: "_possible", True: '_impossible', None: ""}


# %%

P_BUCKETS_RELPLOTS = {
    "full_leaves": np.linspace(0, 200, 20, dtype=int),
    "tp_length": np.arange(0, 30, 1),
    "tree_size": np.arange(0, 2000, 100),
}

P_BUCKETS_DISTPLOTS = {
    "full_leaves": np.unique(np.rint(np.logspace(0, np.log2(200), base=2)).astype(int)),
    "tree_size": np.linspace(0, 2100, 22, dtype=int),
    "leaves": np.linspace(0, 1900, 20, dtype=int),
    "tp_length": np.linspace(0, 45, 45, dtype=int),
}

C1_BUCKETS_RELPLOTS = {
    "full_leaves": np.linspace(0, 500, 50, dtype=int),
    "tp_length": np.arange(0, 11, 1),
    "tree_size": np.arange(0, 800, 100),
}

C1_BUCKETS_DISTPLOTS = {
    "full_leaves": np.unique(np.rint(np.logspace(0, np.log2(600), base=2)).astype(int)),
    "tree_size": np.linspace(0, 800, 9, dtype=int),
    "leaves": np.linspace(0, 1900, 20, dtype=int),
    "tp_length": np.linspace(0, 12, 12, dtype=int),
}


def evaluate(aggregation, filename_prefix, buckets_distplots, buckets_relplots):
    def load_matrices_results(counter, errors):
        KEYS = ["c_nodes", "p_nodes", "full_leaves", "tp_length", "tree_size", "exp_fingerprint", "exp_possible"]
        group = {}

        def group_done():
            counter["all"] += 1
            if "Zanetti" in group["results"] and "CppZanetti" in group["results"] \
                    and group["results"]["Zanetti"]["tree"] != group["results"]["CppZanetti"]["tree"]:
                errors.append({**group, "results": {
                    "Zanetti": group["results"]["Zanetti"],
                    "CppZanetti": group["results"].pop("CppZanetti")
                }, "errors": [
                    "CppZanetti invalid tree"
                ], "name": "CppZanetti", "tree_type": "CppZanetti"})
            if "Zanetti" in group["results"]:
                group["results"]["Zanetti"].pop("tree")
            if "CppZanetti" in group["results"]:
                group["results"]["CppZanetti"].pop("tree")
            return group

        for matrix in tqdm(aggregation):
            if matrix.get("name", "") == "SageMath2":
                matrix["name"] = "SageMath"
            counter[matrix.get("name", "???")] += 1

            # lr = matrix["last_restriction"]
            # if lr.get("exp_fingerprint", "???") != lr.get("fingerprint"):
            #     matrix["errors"].append("fingerprint")
            # if lr.get("exp_possible", "???") != lr.get("possible"):
            #     matrix["errors"].append("possible")

            if matrix["errors"]:
                errors.append(matrix)
                continue

            del matrix["errors"]
            matrix["leaves"] = matrix.pop("cols")
            matrix["last_restriction"]["tp_length"] = matrix["last_restriction"].pop("tpLength")
            matrix["last_restriction"]["full_leaves"] = matrix["last_restriction"].pop("size")
            matrix["last_restriction"]["tree_size"] = \
                matrix["last_restriction"]["c_nodes"] + matrix["last_restriction"]["p_nodes"] + matrix["leaves"]

            if not group or group["parent_id"] != matrix["parent_id"] or group["idx"] != matrix["idx"]:
                if group:
                    yield group_done()
                group = {
                    'leaves': matrix["leaves"],
                    'parent_id': matrix["parent_id"],
                    'idx': matrix["idx"],
                    'results': {},
                    **{k: matrix["last_restriction"][k] for k in KEYS}
                }

            for k in KEYS:
                assert matrix["last_restriction"][k] == group[k], \
                    "type %s, key %s: %s vs %s" % (matrix["name"], k, group, matrix)
                del matrix["last_restriction"][k]
            assert group["exp_possible"] == matrix["last_restriction"]["possible"], \
                "type %s possible: %s vs %s" % (matrix["name"], group, matrix)
            del matrix["last_restriction"]["possible"]
            assert not group["exp_possible"] or group["exp_fingerprint"] == matrix["last_restriction"]["fingerprint"], \
                "type %s fingerprint: %s vs %s" % (matrix["name"], group, matrix)
            del matrix["last_restriction"]["fingerprint"]

            if matrix["name"].endswith("Zanetti"):
                assert matrix["last_restriction"].get("tree", ""), "zanetti tree"

            assert matrix["name"] not in group["results"], \
                "duplicate type %s: %s vs %s" % (matrix["name"], group, matrix)
            group["results"][matrix["name"]] = matrix["last_restriction"]

        yield group_done()


    counter = defaultdict(lambda: 0)
    errors = []
    matrices = pd.json_normalize(load_matrices_results(counter, errors))


    # %%


    def make_stats(counter, errors, matrices):
        errors_counts = Counter((e.get("name", "???"), frozenset(e["errors"])) for e in errors)
        exp_result_counts = dict(zip(*np.unique(matrices["exp_possible"], return_counts=True)))
        table = [{
            "ttype": "all",
            "seen_docs": counter["all"],
            "rows": sum(exp_result_counts.values()),
        }]
        total_vals = 0
        for ttype in counter:
            if ttype == "all": continue
            col = matrices["results.%s.time" % ttype]
            nan_count = np.count_nonzero(np.isnan(col))
            val_count = len(col) - nan_count
            total_vals += val_count
            error_count = sum(v for k, v in errors_counts.items() if k[0] == ttype)
            table.append({
                "ttype": ttype,
                "seen_docs": counter[ttype],
                "errors": error_count,
                "rows": len(col),
                "val_rows": val_count,
                "nan_rows": nan_count,
            })
        return exp_result_counts, total_vals, errors_counts, table


    # todo scipy.stats

    exp_result_counts, total_vals, errors_counts, stats_table = make_stats(counter, errors, matrices)
    print("Expected results:")
    pprint(exp_result_counts)
    print("Errors:")
    pprint(errors_counts)
    print(tabulate(stats_table, headers="keys", tablefmt="pipe"))
    print("Total values:", total_vals)


    # %%


    def distplot_instances(df, x_vals, y_vals, impossible=None):
        if impossible is not None:
            df = df[df["exp_possible"] == (not impossible)]
        # BUCKETS = {
        #     "full_leaves": np.unique(np.rint(np.logspace(0, np.log2(200), base=2)).astype(int)),
        #     "tree_size": np.linspace(0, 2100, 22, dtype=int),
        #     "leaves": np.linspace(0, 1900, 20, dtype=int),
        #     "tp_length": np.linspace(0, 45, 45, dtype=int),
        # }
        x_buckets = pd.cut(df[x_vals], buckets_distplots[x_vals])
        y_buckets = pd.cut(df[y_vals], buckets_distplots[y_vals])
        counts = df.groupby([y_buckets, x_buckets]).size().unstack()
        fig, ax = plt.subplots()
        sns.heatmap(data=counts, cbar=True, cmap=HEATMAP, ax=ax,
                    norm=(Normalize if impossible else LogNorm)(vmin=1, vmax=counts.max().max()))
        if not impossible and x_vals == "full_leaves":
            ax.vlines(3.9, 0, 2100, colors="black")
        ax.invert_yaxis()
        ax.set(xlabel=NAMES[x_vals], ylabel=NAMES[y_vals])
        fig.savefig("%sdistplot_%s_%s%s.png"
                    % (filename_prefix, x_vals, y_vals, IMPOSSIBLE_SUFFIX[impossible]))
        return fig


    print("distplot_instances")
    for x, y in [("full_leaves", "tree_size"), ("full_leaves", "tp_length")]:  # TODO full_leaves 1900 vs 2100
        for p in [None, True, False]:
            print(x, y, p)
            fig = distplot_instances(matrices, x, y, p)
            show_fig(fig)


    # %%


    def histplot_instances(df, impossible=None):
        if impossible is not None:
            df = df[df["exp_possible"] == (not impossible)]
        fig, ax = plt.subplots()
        sns.histplot(df, x="tp_length", discrete=True, ax=ax,
                     palette=HEATMAP.reversed(), hue="tp_length", legend=False)
        ax.set_yscale('log')
        fig.savefig(f"%shistplot_tp_length%s.png" % (filename_prefix, IMPOSSIBLE_SUFFIX[impossible]))
        return fig


    print("histplot_instances")
    for p in [None, True, False]:
        print(p)
        fig = histplot_instances(matrices, p)
        show_fig(fig)
    # %%

    fig, ax = plt.subplots()
    sns.histplot(matrices, x="tp_length", ax=ax, hue="exp_possible",
                 discrete=True, alpha=.6, linewidth=0, hue_order=[True, False])
    ax.set_yscale('log')
    ax.set_xlabel(NAMES["tp_length"])
    ax.get_legend().set_title("Possible")
    fig.savefig(f"%shistplot_tp_length_combined.png" % filename_prefix)
    show_fig(fig)


    # %%


    def get_cols(pre="", suf="", inv=False):
        return [col for col in matrices.dtypes.index if (col.startswith(pre) and col.endswith(suf)) == (not inv)]


    def parse_ttype_results_var(s):
        return s.replace("results.", "").replace(".time", "").replace(".cleanup_time", "")


    results_time = matrices.melt(get_cols(pre="results.", inv=True), get_cols(suf=".time"),
                                 var_name="tree_type", value_name="time")
    results_time["tree_type"] = results_time["tree_type"].apply(parse_ttype_results_var)

    results_cleanup = matrices.melt(["parent_id", "idx"], get_cols(suf=".cleanup_time"),
                                    var_name="tree_type", value_name="cleanup_time")
    results_cleanup["tree_type"] = results_cleanup["tree_type"].apply(parse_ttype_results_var)

    results = pd.merge(results_time, results_cleanup, on=["parent_id", "idx", "tree_type"],
                       how="outer", validate="one_to_one")
    results.fillna({"cleanup_time": 0}, inplace=True)
    results["total_time"] = results["time"] + results["cleanup_time"]
    # results = results.sample(frac=1, random_state=4242)  # shuffle
    results["tree_type"] = pd.Categorical(results["tree_type"], categories=IMPLS_DESC, ordered=True)
    results.sort_values("tree_type", inplace=True)
    print(len(results_time), len(results_cleanup), len(results))

    # %%


    # BUCKETS = {
    #     "full_leaves": np.linspace(0, 200, 20, dtype=int),
    #     "tp_length": np.arange(0, 30, 1),
    #     "tree_size": np.arange(0, 2000, 100),
    # }


    # TODO zoom
    def relplot(df, x_axis, y_axis, impossible, sample_count=None, sample_frac=None):
        fig, ax = plt.subplots()
        if impossible is not None:
            df = df[df["exp_possible"] == (not impossible)]
        x_buckets = pd.cut(df[x_axis], buckets_relplots[x_axis])
        line_data = df[y_axis].groupby([x_buckets, df["tree_type"]]).mean().reset_index()
        line_data[x_axis] = line_data[x_axis].apply(lambda x: x.mid)
        if sample_count is not None:
            df_scatter = df.sample(n=sample_count)
        elif sample_frac is not None:
            df_scatter = df.sample(frac=sample_frac)
        else:
            if INTERACTIVE:
                print("Scatterplot with all samples might take some time...")
            df_scatter = df
        sns.scatterplot(
            x=x_axis, y=y_axis,
            style="tree_type", style_order=IMPLS_ASC, markers=MARKERS,  # s=25,
            hue="tree_type", hue_order=IMPLS_ASC, palette=COLORS,
            data=df_scatter, alpha=0.5,
            legend=True, ax=ax
        )
        sns.lineplot(
            x=x_axis, y=y_axis,
            dashes=False, markers=False,
            linewidth=4,
            hue="tree_type", hue_order=IMPLS_DESC, palette={n: "white" for i, n in enumerate(IMPLS_ASC)},
            data=line_data,
            legend=False, ax=ax
        )
        sns.lineplot(
            x=x_axis, y=y_axis,
            dashes=False, markers=False,
            hue="tree_type", hue_order=IMPLS_DESC, palette=COLORS,
            data=line_data,
            legend=False, ax=ax
        )
        ax.set_xlim(max(buckets_relplots[x_axis]) / (-100), max(buckets_relplots[x_axis]))
        ax.set_ylim(-3000, 100000 if x_axis == "tree_size" else 150000)
        ax.set(ylabel=NAMES[y_axis], xlabel=NAMES[x_axis])
        handles, labels = ax.get_legend_handles_labels()
        ax.legend(handles[::-1], labels[::-1], loc=2)
        fig.savefig(
            "%srelplot_%s_%s%s.png" % (filename_prefix, y_axis, x_axis, IMPOSSIBLE_SUFFIX[impossible]))
        return fig


    # fig = relplot(results, "tree_size", "time", False)
    # show_fig(fig)

    # %%

    print("relplot")
    for x_axis in buckets_relplots:
        # for y_axis in ["time", "cleanup_time", "total_time"]:
        y_axis = "time"
        for impossible in (True, False):
            print(x_axis, y_axis, impossible)
            fig = relplot(results, x_axis, y_axis, impossible)
            show_fig(fig)


    # %%


    def plot_heatmaps_buckets(df, value, impossible=None):
        if impossible is not None:
            df = df[df["exp_possible"] == (not impossible)]
        fig, axes = plt.subplots(nrows=3, ncols=4, sharey=True, sharex=True)

        max_res_size = 150
        max_tree_size = 1000
        nbuckets_x = 10
        nbuckets_y = 10
        tick_steps_x = 3
        tick_steps_y = 3

        df = df[(df["full_leaves"] < max_res_size) & (df["tree_size"] < max_tree_size)]
        for tree_type, ax in zip(IMPLS_DESC, axes.flatten()):
            dfi = df[df["tree_type"] == tree_type]
            groups = dfi.groupby([pd.cut(dfi["tree_size"], nbuckets_y), pd.cut(dfi["full_leaves"], nbuckets_x)])
            vals = groups[value].median()
            # vals[groups[value].count() < 3] = -1
            vmin = vals.min()
            # vmax = np.quantile(groups[value], 0.99)
            sns.heatmap(
                data=vals.unstack(), cmap="flare", cbar=False, xticklabels=tick_steps_x, yticklabels=tick_steps_y, ax=ax,
                vmin=vmin,  # vmax=vmax
            )
            ax.set_xticklabels([int(max_res_size / nbuckets_x) * i for i in range(1, nbuckets_x + 1, tick_steps_x)])
            ax.set_yticklabels([int(max_tree_size / nbuckets_y) * i for i in range(1, nbuckets_y + 1, tick_steps_y)])
            ax.set(xlabel="", ylabel="")
            ax.title.set_text(tree_type)
            ax.invert_yaxis()
            for _, spine in ax.spines.items():
                spine.set_visible(True)

        for ax in axes[:, 0]:
            ax.set_ylabel(NAMES["tree_size"])
        for ax in axes[-1]:
            ax.set_xlabel(NAMES["full_leaves"])

        # FLARE = sns.color_palette("flare", as_cmap=True)
        # axes[7].legend(handles=[
        #     Patch(facecolor=FLARE(0), label='Fastest'),
        #     Patch(facecolor=FLARE(FLARE.N), label='Slowest')
        # ], title='Color map for each implementation',
        #     loc='center', mode='expand', fontsize='small')

        axes[-1][-1].set_axis_off()

        fig.savefig("%sheatmap_%s%s_buckets.png" % (filename_prefix, value, IMPOSSIBLE_SUFFIX[impossible]))
        return fig


    print("heatmaps_buckets")
    fig = plot_heatmaps_buckets(results, "time", impossible=False)
    show_fig(fig)
    # for value in ["time", "cleanup_time", "total_time"]:
    #     print(value)
    #     fig = plot_heatmaps_buckets(results, value, impossible=False)
    #     show_fig(fig)


    # %%

    # def plot_heatmaps_scatter(df):
    #     fig, axes = plt.subplots(nrows=3, ncols=3, sharey=True, sharex=True)
    #
    #     for tree_type, ax in zip(IMPLS_DESC, axes.flatten()):
    #         dfi = df[df["tree_type"] == tree_type]
    #         sns.scatterplot(x="full_leaves", y="tree_size", data=dfi, hue="time", palette="flare", ax=ax,
    #                         legend=False, s=25)
    #         ax.set(xlabel="full_leaves")
    #         ax.title.set_text(tree_type)
    #
    #     fig.savefig("heatmap_scatter.png")
    #     return fig
    #
    #
    # print("heatmaps_scatter")
    # fig = plot_heatmaps_scatter(results)
    # show_fig(fig)

    return results


print("evaluating planarity matrices:")
evaluate(db.matrices_results.aggregate([
    {'$sort': {'parent_id': 1, 'idx': 1}},
    {'$project': {
        'cols': 1,
        'errors': 1,
        'signal': 1,
        'parent_id': 1,
        'idx': 1,
        'name': 1,
        'tree_type': 1,
        'last_restriction': 1
    }},
    {'$project': {
        'last_restriction.uid': 0,
        'last_restriction.exp_uid': 0,
    }},
]), "", P_BUCKETS_DISTPLOTS, P_BUCKETS_RELPLOTS)


print("-------------------------------------")
print("evaluating consecutive-ones matrices:")
results = evaluate(db.matrices_results_2.aggregate([
    {
        '$unwind': {
            'path': '$restrictions'
        }
    },
    {
        '$addFields': {
            'parent_id': {
                '$concat': ["M-m", {'$toString': "$rows"}, "-n", {'$toString': "$cols"}, "-s", {'$toString': "$seed"}]
            }
        }
    },
    {
        '$project': {
            'cols': 1,
            'leaves': "$cols",
            'errors': [],
            'idx': '$restrictions.id',
            'parent_id': 1,
            'name': 1,
            'tree_type': 1,
            'last_restriction': "$restrictions",
            'valid': "$restrictions.valid"
        }
    },
    {
        '$match': {
            'valid': True
        }
    },
    {'$sort': {'parent_id': 1, 'idx': 1}},
    {'$project': {
        'cols': 1,
        'errors': 1,
        'signal': 1,
        'parent_id': 1,
        'idx': 1,
        'name': 1,
        'tree_type': 1,
        'last_restriction': 1
    }},
    {'$project': {
        'last_restriction.uid': 0,
        'last_restriction.exp_uid': 0,
    }},
], allowDiskUse=True), "C1", C1_BUCKETS_DISTPLOTS, C1_BUCKETS_RELPLOTS)


dfs = {}
for impl in IMPLS_ASC_FASTEST:
    dfs[impl] = results[results["tree_type"] == impl][["parent_id", "idx", "full_leaves", "total_time", "tp_length"]]
df_merged = dfs["OGDF"]
for impl in IMPLS_ASC_FASTEST:
    if impl == "OGDF":
        continue
    df_merged = pd.merge(df_merged, dfs[impl], on=["parent_id", "idx", "full_leaves", "tp_length"])
    df_merged[f"speedup.%s/OGDF" % impl] = df_merged["total_time_y"] / df_merged["total_time_x"]
    df_merged = df_merged.drop("total_time_y", 1)
    df_merged = df_merged.rename(columns={"total_time_x": "total_time"})

make_speedupplot(
    df_merged,
    "full_leaves", (0, 1), np.concatenate((np.arange(0, 1000, 100), np.arange(1000, 3500, 250))),
    "C1planaritytest_speedup_size.png")
make_speedupplot(
    df_merged,
    "tp_length", (0, 2), np.arange(0, 66, 6),
    "C1planaritytest_speedup_tp_length.png", loc="lower left")

print("consecutive-ones matrices correctnes stats:")
cursor = db.matrices_results_2.aggregate([
    {
        '$group': {
            '_id': {
                'tree_type': '$tree_type',
                'errors': '$errors'
            },
            'count': {
                '$sum': 1
            }
        }
    }, {
        '$project': {
            '_id': {
                'tree_type': '$_id.tree_type',
                'errors': {
                    '$cond': [
                        {
                            '$eq': [
                                {
                                    '$size': '$_id.errors'
                                }, 0
                            ]
                        }, [
                            'none'
                        ], '$_id.errors'
                    ]
                }
            },
            'count': 1
        }
    }, {
        '$group': {
            '_id': '$_id.tree_type',
            'num_total': {
                '$sum': '$count'
            },
            'errors': {
                '$push': {
                    'k': {
                        '$reduce': {
                            'input': '$_id.errors',
                            'initialValue': '',
                            'in': {
                                '$concat': [
                                    '$$value', {
                                        '$cond': [
                                            {
                                                '$eq': [
                                                    '$$value', ''
                                                ]
                                            }, '', ', '
                                        ]
                                    }, '$$this'
                                ]
                            }
                        }
                    },
                    'v': '$count'
                }
            }
        }
    }, {
        '$project': {
            '_id': 1,
            'num_total': 1,
            'errors': {
                '$arrayToObject': '$errors'
            }
        }
    }, {
        '$addFields': {
            'num_errors': {
                '$subtract': [
                    '$num_total', '$errors.none'
                ]
            }
        }
    }
])

for stat in cursor:
    print(stat)

