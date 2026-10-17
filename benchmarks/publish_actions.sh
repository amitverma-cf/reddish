#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
EXPECTED_SHA=${EXPECTED_SHA:?Pass the measured source revision}
RESULTS_DIR=${RESULTS_DIR:-"$ROOT_DIR/benchmarks/results/comparison"}
[[ ${GITHUB_ACTIONS:-} == true && ${GITHUB_REF:-} == refs/heads/main && $EXPECTED_SHA =~ ^[0-9a-f]{40}$ ]] || { echo 'Publishing requires a main workflow and full measured SHA' >&2; exit 1; }
cd "$ROOT_DIR"
[[ $(git rev-parse HEAD) == "$EXPECTED_SHA" ]] || { echo 'Checkout differs from measured source' >&2; exit 1; }
run_url="${GITHUB_SERVER_URL:-https://github.com}/${GITHUB_REPOSITORY:?}/actions/runs/${GITHUB_RUN_ID:?}"
jq -e --arg revision "$EXPECTED_SHA" --arg run_url "$run_url" '.revision == $revision and .run_url == $run_url' "$RESULTS_DIR/provenance.json" >/dev/null
for file in README.md benchmarks/benchmarks.md; do
  grep -Fq "$EXPECTED_SHA" "$file" || { echo "Missing source revision in $file" >&2; exit 1; }
done
git fetch --no-tags origin refs/heads/main
[[ $(git rev-parse FETCH_HEAD) == "$EXPECTED_SHA" ]] || { echo 'Main moved during measurement; reports remain in the artifact and will not overwrite main' >&2; exit 1; }
git add -- README.md benchmarks/benchmarks.md
if git diff --cached --quiet; then exit 0; fi
[[ $(git diff --cached --name-only | wc -l) == 2 ]] || { echo 'Expected both report files to change together' >&2; exit 1; }
while IFS= read -r file; do
  [[ $file == README.md || $file == benchmarks/benchmarks.md ]] || { echo 'Unexpected staged file' >&2; exit 1; }
done < <(git diff --cached --name-only)
git -c user.name='github-actions[bot]' -c user.email='41898282+github-actions[bot]@users.noreply.github.com' commit -m "Update benchmark reports for ${EXPECTED_SHA:0:7}"
git push origin HEAD:refs/heads/main
