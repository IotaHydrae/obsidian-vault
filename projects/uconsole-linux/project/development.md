# Development practices

## Code changes

Keep changes local and reviewable. Prefer explicit control flow, meaningful names, and small focused functions. Comments should explain hardware constraints or design reasons that are not clear from the code. Do not add abstractions without a concrete use.

## Tests and tools

Tools collect or transform observations; tests decide whether an observation meets a stated requirement. Reuse existing tools before adding another. Do not turn a value observed on one device or boot into an expected value without an independent specification or approved baseline.

Every automated test should identify its oracle: a hardware or software specification, a documented invariant, a user requirement, or mathematically derived expectation. If no oracle exists, report an observation or an `INCONCLUSIVE` result rather than inventing a threshold.

Scriptable tools should use explicit subcommands and options, support `--help`, emit JSON for structured automation, and use documented exit codes. Keep human summaries in the root `Makefile`; keep machine interfaces in `Makefile.agent`.

## Knowledge notes

Put durable project conclusions in `docs/` or a focused `docs/knowledge/` note. Lead with the conclusion, cite the source file/function or command that supports it, separate confirmed facts from observations and unknowns, and omit investigation transcripts and secrets.

## Commits

Write commit messages in English using Linux kernel style: a concise `area: imperative summary`, a body explaining the problem and resulting behavior, and a `Signed-off-by` trailer with the configured author identity. Wrap prose at about 72 columns. Keep unrelated changes in separate commits and record validation without claiming untested hardware behavior.
