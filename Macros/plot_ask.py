#!/usr/bin/env python3
"""A plot asked for in words: a language model writes the plot specification, phistrangeplot checks
and draws it (CLAUDE.md, Plots; Codes/PlotSpec.h).

The model only translates. It is shown the JSON schema of a specification (phistrangeplot schema),
what the files hold (phistrangeplot list --json) and the examples in PlotSpecs/examples, and its
answer is constrained to the schema. What it returns goes through the same reader and the same
checks as a specification written by hand: phistrangeplot refuses what does not exist or does not
make sense, and the refusal is handed back to the model to correct, a few times at most. Before
drawing, phistrangeplot describes what it resolved, and that is what you confirm: a specification
that is valid but not what you meant shows there, in phistrangeplot's words and not the model's.

No dependency beyond the standard library. Three kinds of endpoint:
  --api ollama   a local model through Ollama (default, http://localhost:11434): nothing leaves the
                 machine. E.g. `ollama pull qwen2.5-coder:7b`.
  --api llama    a local model through llama.cpp's llama-server (default http://localhost:8080),
                 e.g. `llama-server -m <model.gguf> -c 16384`; the context is the server's.
  --api openai   any OpenAI-compatible endpoint (--endpoint, key in $PHISTRANGEPLOT_API_KEY). The
                 catalog of the files, their paths and the request then leave the machine.

  python3 Macros/plot_ask.py "K0S trend offline against online, with the ratio" \\
      --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_HighIR_updated_PhiAssocSpectra.root \\
      --file DataFile/pp/DeltaY/FinalResults/Data_OnlineEff_2026_HighIR_updated_PhiAssocSpectra.root \\
      --out Plots/k0s_off_on

Exit codes: 0 drawn (or declined at the confirmation), 1 failed, 3 the model answered that the
request cannot be drawn from these files (phistrangeplot's 3: not in the files).
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXIT_NOT_IN_FILES = 3  # phistrangeplot: what was asked is not in the files
EXIT_CANNOT = 3        # this script: the model answered "cannot", the same meaning
# The model this front end was measured with (Macros/plot_ask_test.py), and its Ollama digest: a
# different model, or the same name pulled again later, is a model that has not been measured.
MEASURED_MODEL = "qwen2.5-coder:7b"
MEASURED_DIGEST = "dae161e27b0e"


def run(cmd, check=True):
    p = subprocess.run(cmd, capture_output=True, text=True)
    if check and p.returncode != 0:
        sys.exit(f"[FATAL] plot_ask: '{' '.join(cmd)}' failed:\n{p.stderr or p.stdout}")
    return p


def call(url, payload=None, headers=None):
    req = urllib.request.Request(url, data=json.dumps(payload).encode() if payload is not None else None,
                                 headers={"Content-Type": "application/json", **(headers or {})})
    try:
        with urllib.request.urlopen(req, timeout=600) as r:
            return json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        sys.exit(f"[FATAL] plot_ask: the model at {url} answered {e.code}: {e.read().decode(errors='replace')[:500]}")
    except urllib.error.URLError as e:
        sys.exit(f"[FATAL] plot_ask: cannot reach the model at {url}: {e}. Is the endpoint running "
                 "(`ollama serve`, or `llama-server -m <model.gguf>`)?")


def prompt_tokens(messages):
    # Two characters per token: compact JSON is dense in tokens. An estimate, on the safe side.
    return sum(len(m["content"]) for m in messages) // 2


def ollama_context(messages):
    # Ollama gives a model 4096 tokens of context unless told otherwise, and cuts a longer prompt
    # without an error: measured on 19 October 2026, the catalog fell off the end and the model
    # invented a file named after the output. Sized here from the prompt, with room for the answer:
    # this estimate, not the warning in ask_model, is what keeps the prompt whole.
    return max(8192, 1024 * (prompt_tokens(messages) // 1024 + 2))


def model_digest(args):
    if args.api != "ollama":
        return None
    for m in call(f"{args.endpoint}/api/tags").get("models", []):
        if m.get("name") == args.model:
            return m.get("digest", "")[:12]
    return None


def check_llama_context(args, messages):
    # llama-server's context is fixed when it starts (-c): read it, and refuse a prompt that does not
    # fit rather than let the server cut it or fail half way.
    props = call(f"{args.endpoint}/props")
    n_ctx = props.get("default_generation_settings", {}).get("n_ctx") or props.get("n_ctx")
    if not n_ctx:
        print("[plot_ask] WARNING: llama-server did not say its context size; the prompt was not checked against it.", file=sys.stderr)
        return
    need = prompt_tokens(messages) + 1024
    if need > n_ctx:
        sys.exit(f"[FATAL] plot_ask: the prompt needs about {need} tokens and llama-server has {n_ctx}: restart it with -c {1024 * (need // 1024 + 2)}.")


def ask_model(args, schema, messages):
    # Temperature 0 and a fixed seed: the same request gives the same specification, as far as the
    # endpoint honours them. The answer's format: any JSON object (--constraint json), or the schema
    # itself (--constraint schema: Ollama's "format", llama-server's and OpenAI's "response_format",
    # which llama.cpp turns into a grammar).
    if args.constraint == "json":
        schema = None
    if args.api == "ollama":
        num_ctx = args.num_ctx or ollama_context(messages)
        r = call(f"{args.endpoint}/api/chat",
                 {"model": args.model, "messages": messages, "stream": False, "format": schema or "json",
                  "options": {"temperature": 0, "seed": args.seed, "num_ctx": num_ctx}})
        # Best effort: Ollama counts only the tokens it evaluated, not a prefix it reused from its
        # cache, so a cut prompt can go unreported here.
        if r.get("prompt_eval_count", 0) >= num_ctx - 16:
            print(f"[plot_ask] WARNING: the prompt filled the whole context ({num_ctx} tokens) and was probably cut; "
                  "the model may not have seen the catalog.", file=sys.stderr)
        return r["message"]["content"]
    if args.api == "llama":
        check_llama_context(args, messages)
    key = os.environ.get("PHISTRANGEPLOT_API_KEY", "")
    r = call(f"{args.endpoint}/v1/chat/completions",
             {"model": args.model, "messages": messages, "temperature": 0, "seed": args.seed,
              "response_format": {"type": "json_schema", "json_schema": {"name": "plot_spec", "schema": schema}} if schema else {"type": "json_object"}},
             {"Authorization": f"Bearer {key}"} if key else None)
    return r["choices"][0]["message"]["content"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("request", help="the plot, in words")
    ap.add_argument("--file", action="append", required=True, help="a file the plot may use (repeat)")
    ap.add_argument("--out", required=True, help="output path without extension: <out>.pdf/.png/.root")
    ap.add_argument("--api", choices=["ollama", "llama", "openai"], default="ollama")
    ap.add_argument("--endpoint", default=None, help="default: http://localhost:11434 (ollama), http://localhost:8080 (llama)")
    ap.add_argument("--model", default=MEASURED_MODEL)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--constraint", choices=["json", "schema"], default="json",
                    help="what the answer is held to while it is written: any JSON object, or the specification's schema")
    ap.add_argument("--num-ctx", type=int, default=None, help="Ollama's context in tokens; by default sized from the prompt")
    ap.add_argument("--retries", type=int, default=2, help="how many times a refused specification is handed back")
    ap.add_argument("--yes", action="store_true", help="draw without asking for confirmation")
    ap.add_argument("--alice", default=None, help="the ALICE label's status; needs --system")
    ap.add_argument("--system", default=None, help="the collision system of the ALICE label")
    ap.add_argument("--phistrangeplot", default=str(REPO / "build" / "phistrangeplot"))
    args = ap.parse_args()
    if args.endpoint is None:
        if args.api == "openai":
            sys.exit("[FATAL] plot_ask: --api openai needs --endpoint.")
        args.endpoint = {"ollama": "http://localhost:11434", "llama": "http://localhost:8080"}[args.api]
    args.endpoint = args.endpoint.rstrip("/")
    if (args.alice is None) != (args.system is None):
        sys.exit("[FATAL] plot_ask: --alice and --system go together: the system is not in the files.")
    tool = args.phistrangeplot
    if not Path(tool).exists():
        sys.exit(f"[FATAL] plot_ask: no phistrangeplot at {tool}. Build it: cmake -B build -S . && cmake --build build --target phistrangeplot")

    digest = model_digest(args)
    if args.model != MEASURED_MODEL or (args.api == "ollama" and digest != MEASURED_DIGEST):
        print(f"[plot_ask] WARNING: model {args.model} ({digest or 'digest unknown'}) is not the one measured "
              f"({MEASURED_MODEL}, {MEASURED_DIGEST}): run Macros/plot_ask_test.py with it before trusting it.", file=sys.stderr)

    # Compact JSON throughout: the indentation would cost a third of the context. The schema goes
    # into the prompt as well as into the constraint, because a constraint restricts the answer
    # without being read: the descriptions of the keys, which say when each applies, reach the model
    # only as text.
    compact = lambda text: json.dumps(json.loads(text), separators=(",", ":"), ensure_ascii=False)
    schema_text = run([tool, "schema"]).stdout
    # The answer is a specification or, when what was asked does not exist, a refusal: a constraint
    # to the specification alone leaves a model no way to say no, and it then substitutes the nearest
    # thing (measured: |dy| < 0.3 asked, 0.5 drawn).
    schema = {"anyOf": [json.loads(schema_text),
                        {"type": "object", "properties": {"cannot": {"type": "string"}}, "required": ["cannot"], "additionalProperties": False}]}
    catalog = compact(run([tool, "list", "--json", *args.file]).stdout)
    examples = "\n".join(compact(p.read_text()) for p in sorted((REPO / "PlotSpecs" / "examples").glob("*.json")))
    instructions = (REPO / "PlotSpecs" / "instructions.txt").read_text()

    messages = [
        {"role": "system", "content": instructions},
        {"role": "user", "content": f"Schema of a specification:\n{compact(schema_text)}\n\nExamples of specifications:\n{examples}\n\n"
                                    f"Catalog of the files:\n{catalog}\n\nRequest: {args.request}"},
    ]

    # "cannot" is a way out for what does not exist, not for a specification phistrangeplot found
    # malformed: offered after a refusal only when the refusal says something is not in the files
    # (measured: a model told it could also say "cannot" gave up on a key it could have added).
    spec_only = json.loads(schema_text)
    current = schema
    work = Path(tempfile.mkdtemp(prefix="plot_ask_"))
    spec_path = work / "spec.json"
    for attempt in range(args.retries + 1):
        described = None
        answer = ask_model(args, current, messages)
        try:
            spec = json.loads(answer)
        except json.JSONDecodeError as e:
            problem = f"The answer is not valid JSON ({e})."
        else:
            if isinstance(spec, dict) and "cannot" in spec:
                print(f"[plot_ask] the model says this cannot be drawn from these files: {spec['cannot']}", file=sys.stderr)
                sys.exit(EXIT_CANNOT)
            spec_path.write_text(json.dumps(spec, indent=2) + "\n")
            described = run([tool, "describe", str(spec_path)], check=False)
            if described.returncode == 0:
                break
            problem = (described.stderr or described.stdout).strip()
        print(f"[plot_ask] attempt {attempt + 1} refused:\n{problem}\n", file=sys.stderr)
        missing = described is not None and described.returncode == EXIT_NOT_IN_FILES
        current = schema if missing else spec_only
        hint = ("Correct it, or answer with \"cannot\" if what was asked does not exist in the catalog." if missing
                else "Correct the specification.")
        messages += [{"role": "assistant", "content": answer},
                     {"role": "user", "content": f"phistrangeplot refused this specification:\n{problem}\n{hint}"}]
    else:
        sys.exit(f"[FATAL] plot_ask: no acceptable specification after {args.retries + 1} attempts; the last is in {spec_path}.")

    print(described.stdout)
    # The specification is kept next to the plot: in its .root as its recipe when drawn, as a file when not.
    kept = Path(f"{args.out}.spec.json")
    kept.parent.mkdir(parents=True, exist_ok=True)
    kept.write_text(spec_path.read_text())
    if not args.yes and input("Draw it? [y/N] ").strip().lower() not in ("y", "yes"):
        print(f"Not drawn. The specification is in {kept}: `phistrangeplot render {kept} --out {args.out}` draws it.")
        return
    note = f"asked: {args.request}\nmodel: {args.model} ({digest or args.api})"
    cmd = [tool, "render", str(kept), "--out", args.out, "--note", note]
    if args.alice:
        cmd += ["--alice", args.alice, "--system", args.system]
    rendered = run(cmd, check=False)
    print(rendered.stdout, end="")
    print(rendered.stderr, end="", file=sys.stderr)
    sys.exit(rendered.returncode)


if __name__ == "__main__":
    main()
