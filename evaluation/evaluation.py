import json
import re
import shlex
import tempfile

import math
import sys
import textwrap
from json import JSONDecodeError
from pathlib import Path

import click
import pymongo
import sh
import time
from pymongo import ASCENDING as ASC

EVALUATION_DIR = Path(__file__).absolute().parent
MAIN_DIR = EVALUATION_DIR.parent
BUILD_DIR = MAIN_DIR.joinpath("build-release")
MATRIX_DIR = EVALUATION_DIR.joinpath("matrices")
JAVA_DIR = MAIN_DIR.joinpath("evaluation/javaEvaluation")
JAVA_EVAL_RESTRS_PATH = JAVA_DIR.joinpath("build/libs/testRestrictions.jar")
JAVA_EVAL_PLAN_PATH = JAVA_DIR.joinpath("build/libs/testPlanarity.jar")
SAGEMATH_PATH = EVALUATION_DIR.joinpath("sagemath/main.py")

db = pymongo.MongoClient(
    "pc-tree-mongo", connectTimeoutMS=2000 
).pc_tree # adapt to your needs


def bake_srun(**kwargs):
    return sh.srun.bake(
        #constraint="chimaira", # adapt to your needs
        #mem="10G",
        **kwargs
    )


def bake_sbatch(**kwargs):
    return sh.sbatch.bake(
        #constraint="chimaira",
        #mem="10G",
        exclusive="user",
        **kwargs
    )


def bulk_write_batched(coll, generator, batch_size=100):
    tasks = []
    try:
        for task in generator:
            tasks.append(task)
            if len(tasks) > batch_size:
                yield coll.bulk_write(tasks)
                tasks.clear()
    finally:
        if tasks:
            yield coll.bulk_write(tasks)
            tasks.clear()


class JsonParamType(click.ParamType):
    name = "json"

    def convert(self, value, param, ctx):
        if isinstance(value, (dict, list, int, float, bool)):
            return value
        try:
            return json.loads(value)
        except JSONDecodeError as e:
            self.fail("could not load json value: " + str(e), param, ctx, )


JSONType = JsonParamType()


@click.group()
def cli():
    pass


@cli.command()
@click.option("--local", is_flag=True)
@click.option("--clean", is_flag=True)
def compile(local, clean):
    tasks = ["clean"] if clean else []
    sh.Command("gradlew", [JAVA_DIR])(*tasks, "build", _cwd=JAVA_DIR, _out=sys.stdout, _err=sys.stderr)
    rsh = sh
    if not local:
        rsh = bake_srun(time="00:05:00", job_name="pctree compile")
    rsh.make(*tasks, "all", "-j", 6 if local else 18, _cwd=BUILD_DIR, _out=sys.stdout, _err=sys.stderr)

@cli.command(context_settings=dict(ignore_unknown_options=True))
@click.argument('method', type=click.Choice(['planarity', 'tree'], case_sensitive=False))
@click.argument('args', nargs=-1, type=click.UNPROCESSED)
def make_restrictions(method, args):
    MATRIX_DIR.mkdir(exist_ok=True)

    def parse_jsons():
        cmd = {"planarity": sh.Command("make_restrictions_planarity", [BUILD_DIR]),
               "tree": sh.Command("make_restrictions_tree", [BUILD_DIR])}
        for line in cmd[method](args, MATRIX_DIR, _iter=True):
            if not line.startswith(("GRAPH:", "TREE:", "MATRIX:", "RESTRICTION:")):
                if len(line) > 500:
                    print("Can't parse line:", line[:250], "...", line[250:])
                else:
                    print("Can't parse line:", line)
                continue
            type, data = line.split(":", 1)
            data = json.loads(data)
            if type in ("GRAPH", "TREE"):
                print(type, db.data_sources.replace_one({"id": data["id"]}, data, upsert=True).raw_result)
            elif type == "MATRIX":
                print(type, db.matrices.replace_one({"id": data["id"]}, data, upsert=True).raw_result)
            else:
                yield pymongo.ReplaceOne({"id": data["id"]}, data, upsert=True)

    for res in bulk_write_batched(db.restrictions, parse_jsons()):
        raw = dict(res.bulk_api_result)
        raw["upserted"] = len(raw["upserted"])
        print(raw)



@cli.command()
@click.argument("calls", nargs=-1)
@click.option("--calls-from", "-f", multiple=True, type=click.File("rt"))
@click.option("--nodes", multiple=True)
@click.option("--nodes-from", default=1000)
@click.option("--nodes-to", default=21000)
@click.option("--nodes-step", default=1000)
@click.option("--planar", is_flag=True)
@click.option("--seed", default=0)
@click.option("--sbatch-options", default="")
def batch_make_restrictions(calls, calls_from, nodes, nodes_from, nodes_to, nodes_step, planar, seed, sbatch_options):
    db.data_sources.create_index(
        [("id", ASC)], name="id", unique=True, background=True)
    db.matrices.create_index(
        [("id", ASC)], name="id", unique=True, background=True)
    db.matrices.create_index(
        [("parent_id", ASC), ("idx", ASC)], name="parent_id", unique=True, background=True)
    db.restrictions.create_index(
        [("id", ASC)], name="id", unique=True, background=True)
    db.restrictions.create_index(
        [("parent_id", ASC), ("idx", ASC)], name="parent_id", unique=True, background=True)
    calls = [shlex.split(l) for l in calls]
    if calls_from:
        calls.extend(shlex.split(l) for f in calls_from for l in f)
    if not calls:
        if not nodes:
            nodes = range(nodes_from, nodes_to, nodes_step)
        edges = [(lambda n: 2 * n), (lambda n: 3 * n - 6)]
        calls = [
            ("planarity", "-n", n, "-m", m(n), "-s", seed, *(["-p"] if planar else []))
            for n in nodes
            for m in edges
        ]

    script = textwrap.dedent(
        """
        #!/bin/bash
        TASKS=({calls_str})
        {exec} -u {file} make-restrictions {args}
        """.format(
            exec=sys.executable, file=__file__,
            calls_str=" ".join(shlex.quote(" ".join(str(a) for a in call)) for call in calls),
            args="${TASKS[$SLURM_ARRAY_TASK_ID]}"
        )).strip()
    sbatch = bake_sbatch(array="0-%s" % (len(calls) - 1), job_name="pctree make-restrictions", _in=script)
    sbatch(*shlex.split(sbatch_options), _out=sys.stdout, _err=sys.stderr)


@cli.command()
@click.option("--type", "-t", "ttypes", multiple=True, default=["UFPC"])
@click.argument("infiles", required=True, nargs=-1)
def test_restrictions(ttypes, infiles):
    test_restrictions = sh.Command("test_restrictions", [BUILD_DIR])
    #likwid = sh.Command("likwid-perfctr", [BUILD_DIR]).bake("-C", 0, "-g", "CYCLE_STALLS", "-o", "likwid.txt", "-m")
    # likwid = sh.Command("likwid-perfctr").bake("-C", "S0:2", "-g", "CYCLE_STALLS", "-o", "likwid.txt", "-m")
    java = sh.Command("java").bake("-jar", JAVA_EVAL_RESTRS_PATH)
    test_commands = {
        "UFPC": test_restrictions.bake("-t", "UFPC"),
        "HsuPC": test_restrictions.bake("-t", "HsuPC"),
        "BiVoc": test_restrictions.bake("-t", "BiVoc"),
        "Reisle": test_restrictions.bake("-t", "Reisle"),
        "Gregable": test_restrictions.bake("-t", "Gregable"),
        "OGDF": test_restrictions.bake("-t", "OGDF"),
        "GraphSet": test_restrictions.bake("-t", "GraphSet"),
        "CppZanetti": test_restrictions.bake("-t", "CppZanetti"),

        "SageMath2": sh.Command("python3").bake(SAGEMATH_PATH),
        #"SageMath2-pypy": sh.Command("pypy3").bake(SAGEMATH_PATH, "-r", 3, "-n", "SageMath2-pypy"),

        "GraphTea": java.bake("-t", "GraphTea"),
        "JGraphEd": java.bake("-t", "JGraphEd"),
        "Zanetti": java.bake("-t", "Zanetti"),

        #"UFPC-likwid": likwid.bake(sh.which("test_restrictions_likwid", [BUILD_DIR]),
        #                           "-a", "-t", "UFPC", "-n", "UFPC-likwid"),
    }

    for ttype in ttypes:
        def parse_jsons():
            if ttype in ("Zanetti", "JGraphEd", "GraphTea"):
                cmd = test_commands[ttype]("-n", ttype, infiles)
                for l in cmd:
                    j = json.loads(l)
                    yield pymongo.ReplaceOne({"id": j["id"]}, j, upsert=True)
            else:
                for file in infiles:
                    cmd = test_commands[ttype]("-n", ttype, file)
                    for l in cmd:
                        j = json.loads(l)
                        yield pymongo.ReplaceOne({"id": j["id"]}, j, upsert=True)

        for res in bulk_write_batched(db.matrices_results, parse_jsons()):
            raw = dict(res.bulk_api_result)
            raw["upserted"] = len(raw["upserted"])
            print(raw)


@cli.command()
@click.option("--type", "-t", "ttypes", multiple=True, default=["UFPC", "HsuPC", "BiVoc", "Reisle", "Gregable", "OGDF", "GraphSet", "CppZanetti", "Zanetti", "JGraphEd"])
@click.option("--query", "-q", type=JSONType, default={})
@click.option("--sort", "-o", type=JSONType, default={})
@click.option("--limit", "-l", type=int, default=0)
@click.option("--skip", "-s", type=int, default=0)
@click.option("--batchsize", "-b", type=int, default=1000)
def batch_test_restrictions(ttypes, query, sort, limit, skip, batchsize):
    db.matrices_results.create_index(
        [("id", ASC)], name="id", unique=True, background=True)
    db.matrices_results.create_index(
        [("parent_id", ASC), ("idx", ASC)], name="parent_id", background=True)
    db.restrictions_results.create_index(
        [("id", ASC)], name="id", unique=True, background=True)
    db.restrictions_results.create_index(
        [("parent_id", ASC), ("idx", ASC)], name="parent_id", unique=True, background=True)
    infiles = []
    cur = db.data_sources.find(query, sort=sort, limit=limit, skip=skip)
    def create_job_lookup(files):
        with tempfile.NamedTemporaryFile(mode="w", delete=False, dir=EVALUATION_DIR) as f:
            i = 0
            for file in files:
                i += 1
                f.write("%s " % file)
                if i % batchsize == 0:
                    f.write("\n")
            return f.name

    def start_jobs(files, tmpfile):
        for ttype in ttypes:
            script = textwrap.dedent(
                """
                #!/bin/bash
                FILES=$(sed "$((SLURM_ARRAY_TASK_ID + 1))q;d" {lookup})
                {exec} -u {file} test-restrictions --type {ttype} ${{FILES}}
                """.format(
                    exec=sys.executable, file=__file__,
                    lookup=tmpfile,
                    **locals()
                )).strip()
            sbatch = bake_sbatch(array="0-%s" % (math.ceil(len(files) / batchsize) - 1), job_name="pctree %s test-planarity %s" % (ttype, tmpfile.rpartition("/")[2]), _in=script)
            res = str(sbatch()).strip()
            match = re.fullmatch("Submitted batch job ([0-9]+)", res)
            jobid = match.group(1)
            print(jobid)
            time.sleep(1)

    for graph in cur:
        for matrixFile in graph["matrices"]:
            infiles.append(matrixFile)
    tmpfile = create_job_lookup(infiles)
    start_jobs(infiles, tmpfile)



@cli.command()
@click.option("--type", "-t", "ttypes", multiple=True, default=["UFPC"])
@click.argument("infile", required=True,
                type=click.Path(exists=True, dir_okay=False, resolve_path=True))
@click.argument("outfile", required=False,
                type=click.Path(exists=False, dir_okay=False, resolve_path=True, writable=True))
@click.option("--size", "-s", help="minRestrictionSize", type=int, default=25)
@click.option("--checkpoint", "-c", help="checkpointInterval", type=int, default=1000)
@click.option("--repetitions", "-r", type=int, default=3)
def test_planarity(ttypes, infile, outfile, size, checkpoint, repetitions):
    db.planarity_results.create_index(
        [("id", ASC)], name="id", unique=True, background=True)
    db.planarity_results.create_index(
        [("file", ASC), ("type", ASC), ("index", ASC)], name="file_type_index", unique=True, background=True)
    db.planarity_results.create_index(
        [("file", ASC), ("index", ASC), ("type", ASC)], name="file_index_type", unique=True, background=True)

    has_zanetti = "Zanetti" in ttypes
    zanetti = sh.Command("java").bake("-jar", JAVA_EVAL_PLAN_PATH)
    test_planarity = sh.Command("test_planarity", [BUILD_DIR])

    for ttype in ttypes:
        cur_outfile = outfile
        if not outfile and (ttype == "Zanetti" or (has_zanetti and ttype == ttypes[0])):
            cur_outfile = infile + ".jsons"
        if ttype == "Zanetti":
            cmd = zanetti("-s", size, "-c", checkpoint, "-r", repetitions, cur_outfile, _iter=True)
        else:
            cmd = test_planarity("-t", ttype, "-s", size, "-c", checkpoint, infile, cur_outfile, _iter=True)
        print(cmd.ran)

        def parse_jsons():
            for l in cmd:
                j = json.loads(l)
                j["id"] = "/".join((j["file"], j["type"], str(j["index"])))
                yield pymongo.ReplaceOne({"id": j["id"]}, j, upsert=True)

        for res in bulk_write_batched(db.planarity_results, parse_jsons()):
            raw = dict(res.bulk_api_result)
            raw["upserted"] = len(raw["upserted"])
            print(raw)


@cli.command()
@click.option("--type", "-t", "ttypes", multiple=True, default=["UFPC", "HsuPC", "OGDF", "CppZanetti", "Zanetti"])
@click.argument("infiles", required=True, nargs=-1)
@click.option("--size", "-s", help="minRestrictionSize", type=int, default=25)
@click.option("--checkpoint", "-c", help="checkpointInterval", type=int, default=1000)
@click.option("--repetitions", "-r", type=int, default=3)
def batch_test_planarity(ttypes, infiles, size, checkpoint, repetitions):
    has_zanetti = "Zanetti" in ttypes
    jobids = []
    for ttype in ttypes:
        script = textwrap.dedent(
            """
            #!/bin/bash
            TASKS=({infiles_str})
            {exec} -u {file} test-planarity ${{TASKS[$SLURM_ARRAY_TASK_ID]}} {args} \
                --type {ttype} --size {size} --checkpoint {checkpoint} --repetitions {repetitions}
            """.format(
                exec=sys.executable, file=__file__,
                infiles_str=" ".join([shlex.quote(file) for file in infiles]),
                args="${TASKS[$SLURM_ARRAY_TASK_ID]}.jsons" if has_zanetti and not jobids else "",
                **locals()
            )).strip()
        sbatch = bake_sbatch(array="0-%s" % (len(infiles) - 1), job_name="pctree %s test-planarity" % ttype, _in=script)
        if ttype == "Zanetti":
            sbatch = sbatch.bake(dependency="aftercorr:" + jobids[0])
        res = str(sbatch()).strip()
        match = re.fullmatch("Submitted batch job ([0-9]+)", res)
        jobid = match.group(1)
        print(jobid)
        jobids.append(jobid)


if __name__ == '__main__':
    cli()
