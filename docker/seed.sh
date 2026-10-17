#!/usr/bin/env bash
# Seeds the local Redmine test environment with realistic data in one go, see README.md.
#
#   docker/seed.sh           # add the demo data (existing demo projects are left alone)
#   docker/seed.sh --reset   # delete the demo projects first, then seed them from scratch
#
# Any further arguments are passed to seed_redmine.py (e.g. --weeks 8, --seed 7).
set -euo pipefail

cd "$(dirname "$0")/.."
COMPOSE=(docker compose -f docker/docker-redmine.yml)
# Matches REDMINE_SECRET_KEY_BASE in docker-redmine.yml; `exec` doesn't inherit the container's
# entrypoint environment, and `rails runner` needs it.
RUNNER=("${COMPOSE[@]}" exec -T -e SECRET_KEY_BASE=supersecretkey redmine bin/rails runner /tmp/seed_redmine_config.rb)

echo "== Server-side configuration (activities, custom fields, REST API)"
"${COMPOSE[@]}" cp docker/seed_redmine_config.rb redmine:/tmp/seed_redmine_config.rb >/dev/null
setup_output=$("${RUNNER[@]}" setup 2>/dev/null)
grep -v '^ADMIN_API_KEY=' <<<"$setup_output"
api_key=$(sed -n 's/^ADMIN_API_KEY=//p' <<<"$setup_output")
if [[ -z "$api_key" ]]; then
  echo "Could not read the admin API key from Redmine." >&2
  exit 1
fi

echo "== Demo data"
# PYTHON can point at a virtualenv with docker/requirements.txt installed.
REDMINE_API_KEY="$api_key" "${PYTHON:-python3}" docker/seed_redmine.py "$@"

echo "== Per-project settings"
"${RUNNER[@]}" finish 2>/dev/null
