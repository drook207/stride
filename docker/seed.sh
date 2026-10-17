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
CONFIG_DIR=/tmp/qredmine-seed

# Runs seed_redmine_config.rb in the container. `exec` doesn't inherit the container's entrypoint
# environment, so SECRET_KEY_BASE (needed by `rails runner`) is taken from the container's own
# REDMINE_SECRET_KEY_BASE. Errors are shown if it fails; otherwise only its regular output is.
run_config() {
  local errors
  errors=$(mktemp)
  # The single quotes are intended: the variables expand inside the container, not here.
  # shellcheck disable=SC2016
  if ! "${COMPOSE[@]}" exec -T redmine sh -c \
      'SECRET_KEY_BASE="$REDMINE_SECRET_KEY_BASE" exec bin/rails runner "$0/seed_redmine_config.rb" "$1"' \
      "$CONFIG_DIR" "$1" 2>"$errors"; then
    echo "seed_redmine_config.rb $1 failed:" >&2
    grep -v 'warning:' "$errors" >&2 || true
    rm -f "$errors"
    exit 1
  fi
  rm -f "$errors"
}

echo "== Server-side configuration (activities, custom fields, REST API)"
"${COMPOSE[@]}" exec -T redmine mkdir -p "$CONFIG_DIR" 2>/dev/null
for file in seed_redmine_config.rb seed_activity_overrides.json; do
  "${COMPOSE[@]}" cp "docker/$file" "redmine:$CONFIG_DIR/$file" >/dev/null 2>&1
done
setup_output=$(run_config setup)
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
run_config finish
