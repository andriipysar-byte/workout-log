#!/usr/bin/env bash
# Registers an ephemeral runner, serves one job, and exits; the compose restart
# policy starts it again. Ephemeral keeps state from one job out of the next.
set -euo pipefail

REPO="${REPO:-andriipysar-byte/workout-log}"
: "${ACCESS_TOKEN:?set ACCESS_TOKEN in ci/runner/.env (see ci/runner/README.md)}"

api_token() {
    curl -fsSL -X POST \
        -H "Authorization: Bearer $ACCESS_TOKEN" \
        -H "Accept: application/vnd.github+json" \
        "https://api.github.com/repos/$REPO/actions/runners/$1" | jq -r .token
}

# A restarted container keeps its filesystem, including the previous
# registration and checkout.
rm -rf .runner .credentials .credentials_rsaparams _work

./config.sh --unattended --ephemeral --replace \
    --url "https://github.com/$REPO" \
    --token "$(api_token registration-token)" \
    --name "${RUNNER_NAME:-workout-log-$(hostname)}" \
    --labels workout-log \
    --work _work

deregister() {
    ./config.sh remove --token "$(api_token remove-token)" || true
}
trap 'deregister; exit 143' TERM INT

./run.sh &
wait $!
