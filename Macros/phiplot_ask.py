#!/usr/bin/env python3
"""A plot asked for in words: a language model writes the plot specification, phiplot checks
and draws it (README.md, Plots on demand).

The model only translates. It is shown the JSON schema of a specification (phiplot schema),
what the files hold (phiplot list --json) and the examples in PlotSpecs/examples, and its
answer is constrained to the schema. What it returns goes through the same parser and the
same checks as a specification written by hand: phiplot refuses what does not exist or does
not make sense, and the refusal is handed back to the model to correct, a few times at most.
Before drawing, phiplot describes in plain words what it resolved, and that is what you
confirm - a specification that is valid but not what you meant shows there, in phiplot's
words and not the model's.

No dependency beyond the standard library. Two kinds of endpoint:
  --api ollama   a local model through Ollama (default, http://localhost:11434): offline, nothing
                 leaves the machine. E.g. `ollama pull qwen2.5-coder:7b`.
  --api openai   any OpenAI-compatible endpoint (--endpoint, key in $PHIPLOT_API_KEY).

  python3 Macros/phiplot_ask.py "K0S trend 2024 against 2026, with the ratio" \\
      --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2024_PhiAssocSpectra.root \\
      --file DataFile/pp/DeltaY/FinalResults/Data_OfflineEff_2026_PhiAssocSpectra.root \\
      --out Plots/k0s_years
"""

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
EXIT_NOT_IN_FILES = 3  # phiplot.cpp, kExitNotInFiles: what was asked is not in the files

# Only what the schema does not already say: what each key means, and when "mult",
# "species_den" and "divide_option" apply, is in the descriptions of the schema itself
# (PlotSpec's table), so it is written once.
INSTRUCTIONS = """You write plot specifications for the tool phiplot, as JSON that follows the schema you are given; read the description of every key.
- Use only what the catalog lists: its file paths exactly as written, its kinds, species, dy values and multiplicity intervals.
- If the request asks for something the catalog does not have (a species, a dy value, an interval), do not substitute the nearest: answer {"cannot": "<what is missing, and what there is>"} instead.
- Add "ratio" only if the request asks to compare or divide; its reference is the series the request divides by.
- Give each series a short "label" when the request names what the series are (years, methods); otherwise leave it out.
Answer with the JSON object only."""


def run(cmd, check=True):
    p = subprocess.run(cmd, capture_output=True, text=True)
    if check and p.returncode != 0:
        sys.exit(f"[FATAL] phiplot_ask: '{' '.join(cmd)}' failed:\n{p.stderr or p.stdout}")
    return p


def post(url, payload, headers=None):
    req = urllib.request.Request(url, data=json.dumps(payload).encode(), headers={"Content-Type": "application/json", **(headers or {})})
    try:
        with urllib.request.urlopen(req, timeout=600) as r:
            return json.loads(r.read().decode())
    except urllib.error.URLError as e:
        sys.exit(f"[FATAL] phiplot_ask: cannot reach the model at {url}: {e}. Is the endpoint running (for Ollama: `ollama serve`)?")


def context_for(messages):
    # Ollama gives a model 4096 tokens of context unless told otherwise, and cuts a longer
    # prompt without an error: measured on 19 October 2026, the catalog fell off the end and the
    # model invented a file named after the output. Sized here from the prompt, at two characters
    # per token (compact JSON is dense in tokens), with room for the answer: this estimate, not
    # the warning below, is what keeps the prompt whole.
    chars = sum(len(m["content"]) for m in messages)
    return max(8192, 1024 * (chars // 2 // 1024 + 2))


def ask_model(args, schema, messages):
    # Temperature 0 and a fixed seed: the same request gives the same specification, as far as
    # the endpoint honours them.
    if args.api == "ollama":
        num_ctx = context_for(messages)
        r = post(f"{args.endpoint.rstrip('/')}/api/chat",
                 {"model": args.model, "messages": messages, "stream": False, "format": schema,
                  "options": {"temperature": 0, "seed": args.seed, "num_ctx": num_ctx}})
        # Best effort: Ollama counts only the tokens it evaluated, not a prefix it reused from its
        # cache, so a cut prompt can go unreported here.
        if r.get("prompt_eval_count", 0) >= num_ctx - 16:
            print(f"[phiplot_ask] WARNING: the prompt filled the whole context ({num_ctx} tokens) and was probably cut; "
                  "the model may not have seen the catalog.", file=sys.stderr)
        return r["message"]["content"]
    key = os.environ.get("PHIPLOT_API_KEY", "")
    r = post(f"{args.endpoint.rstrip('/')}/v1/chat/completions",
             {"model": args.model, "messages": messages, "temperature": 0, "seed": args.seed,
              "response_format": {"type": "json_schema", "json_schema": {"name": "plot_spec", "schema": schema}}},
             {"Authorization": f"Bearer {key}"} if key else None)
    return r["choices"][0]["message"]["content"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("request", help="the plot, in words")
    ap.add_argument("--file", action="append", required=True, help="a PhiAssocSpectra file the plot may use (repeat)")
    ap.add_argument("--out", required=True, help="output path without extension; the specification is saved as <out>.spec.json")
    ap.add_argument("--api", choices=["ollama", "openai"], default="ollama")
    ap.add_argument("--endpoint", default=None, help="default: http://localhost:11434 for ollama")
    ap.add_argument("--model", default="qwen2.5-coder:7b")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--retries", type=int, default=2, help="how many times a refused specification is handed back")
    ap.add_argument("--yes", action="store_true", help="draw without asking for confirmation")
    ap.add_argument("--phiplot", default=str(REPO / "build" / "phiplot"))
    args = ap.parse_args()
    if args.endpoint is None:
        if args.api == "openai":
            sys.exit("[FATAL] phiplot_ask: --api openai needs --endpoint.")
        args.endpoint = "http://localhost:11434"
    if not Path(args.phiplot).exists():
        sys.exit(f"[FATAL] phiplot_ask: no phiplot at {args.phiplot}. Build it: cmake -B build -S . && cmake --build build --target phiplot")

    # Compact JSON throughout: the indentation would cost a third of the context. The schema
    # goes into the prompt as well as into the constraint, because a constraint restricts the
    # answer without being read: the descriptions of the keys, which say when each applies,
    # reach the model only as text.
    compact = lambda text: json.dumps(json.loads(text), separators=(",", ":"), ensure_ascii=False)
    schema_text = run([args.phiplot, "schema"]).stdout
    # The answer is a specification or, when what was asked does not exist, a refusal: a
    # constraint to the specification alone leaves a model no way to say no, and it then
    # substitutes the nearest thing (measured: |dy| < 0.3 asked, 0.5 drawn).
    schema = {"anyOf": [json.loads(schema_text),
                        {"type": "object", "properties": {"cannot": {"type": "string"}}, "required": ["cannot"], "additionalProperties": False}]}
    catalog = compact(run([args.phiplot, "list", "--json", *args.file]).stdout)
    examples = "\n".join(compact(p.read_text()) for p in sorted((REPO / "PlotSpecs" / "examples").glob("*.json")))

    messages = [
        {"role": "system", "content": INSTRUCTIONS},
        {"role": "user", "content": f"Schema of a specification:\n{compact(schema_text)}\n\nExamples of specifications:\n{examples}\n\n"
                                    f"Catalog of the files:\n{catalog}\n\nOutput: {args.out}\n\nRequest: {args.request}"},
    ]
    spec_path = Path(f"{args.out}.spec.json")
    spec_path.parent.mkdir(parents=True, exist_ok=True)

    # "cannot" is a way out for what does not exist, not for a specification phiplot found
    # malformed: offered after a refusal only when the refusal says an object is missing
    # (measured: a model told it could also say "cannot" gave up on a missing species_den).
    spec_only = json.loads(schema_text)
    current = schema
    for attempt in range(args.retries + 1):
        described = None
        answer = ask_model(args, current, messages)
        try:
            spec = json.loads(answer)
        except json.JSONDecodeError as e:
            problem = f"The answer is not valid JSON ({e})."
        else:
            if isinstance(spec, dict) and "cannot" in spec:
                sys.exit(f"[phiplot_ask] the model says this cannot be drawn from these files: {spec['cannot']}")
            # The output path is the caller's, not the model's to choose.
            if isinstance(spec, dict) and spec.get("output") != args.out:
                spec["output"] = args.out
            spec_path.write_text(json.dumps(spec, indent=2) + "\n")
            described = run([args.phiplot, "describe", str(spec_path)], check=False)
            if described.returncode == 0:
                break
            problem = (described.stderr or described.stdout).strip()
        print(f"[phiplot_ask] attempt {attempt + 1} refused:\n{problem}\n", file=sys.stderr)
        missing = described is not None and described.returncode == EXIT_NOT_IN_FILES
        current = schema if missing else spec_only
        hint = ("Correct it, or answer with \"cannot\" if what was asked does not exist in the catalog." if missing
                else "Correct the specification.")
        messages += [{"role": "assistant", "content": answer},
                     {"role": "user", "content": f"phiplot refused this specification:\n{problem}\n{hint}"}]
    else:
        sys.exit(f"[FATAL] phiplot_ask: no acceptable specification after {args.retries + 1} attempts; the last is in {spec_path}.")

    print(described.stdout)
    if not args.yes and input("Draw it? [y/N] ").strip().lower() not in ("y", "yes"):
        print(f"Not drawn. The specification is in {spec_path}: `phiplot render {spec_path}` draws it.")
        return
    rendered = run([args.phiplot, "render", str(spec_path)], check=False)
    print(rendered.stdout, end="")
    print(rendered.stderr, end="", file=sys.stderr)
    sys.exit(rendered.returncode)


if __name__ == "__main__":
    main()
