# Local Redmine test environment

Spins up a throwaway Redmine + MySQL instance for manually exercising
QRedmineTimeTracker against a real server, and seeds it with demo projects,
issues, users and time entries via `seed_redmine.py`.

## 1. Start Redmine

```bash
docker compose -f docker/docker-redmine.yml up -d
```

Give it a minute on first start (Redmine runs its DB migrations before it
starts serving requests).

## 2. Load Redmine's default configuration data

The official Redmine image does **not** seed trackers, issue statuses,
priorities, roles or default workflows on its own - without this step there
are zero trackers/statuses/roles to work with at all, and creating issues or
project memberships fails outright:

```bash
docker compose -f docker/docker-redmine.yml exec -e SECRET_KEY_BASE=supersecretkey redmine \
  rake redmine:load_default_data REDMINE_LANG=en
```

(`SECRET_KEY_BASE` has to be passed explicitly here - it matches the
`REDMINE_SECRET_KEY_BASE` compose value above, and `docker compose exec`
doesn't inherit the container's own entrypoint-derived environment.)

## 3. One-time manual setup

Redmine's REST API is disabled by default, and the built-in `admin` account
forces a password change on first login - neither of these can be done over
the REST API itself, so they need one manual pass through the web UI:

1. Open <http://localhost:8080>, log in as `admin` / `admin`, and set a new
   admin password when prompted.
2. *Administration -> Settings -> API* -> check **Enable REST web service**
   -> *Save*.
3. *My account* (top right) -> **API access key** -> *Show* -> copy it.

## 4. Seed test data

```bash
pip install -r docker/requirements.txt
REDMINE_API_KEY=<the key from step 3.3> python3 docker/seed_redmine.py
```

This creates 3 demo projects, a handful of demo users (`alice`/`bob`/`carol`,
password `TestPass123!`), a spread of issues across trackers/statuses/
priorities, and a couple of weeks of realistic time entries against them. It
prints each demo user's own API key at the end, if you want to test the app
as a non-admin.

The script is safe to re-run: existing users/projects are detected and left
alone, and issue/time-entry seeding for a project is skipped once it already
has enough of either. Run `python3 docker/seed_redmine.py --help` for all
options (custom URL, issue count per project, a `--seed` for reproducible
data, etc).

## 5. Point the app at it

In QRedmineTimeTracker's Settings view, set the Redmine URL to
`http://localhost:8080/` and paste in an API key from step 3.3 (admin) or the
script's output (a demo user).
