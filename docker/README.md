# Local Redmine test environment

Spins up a throwaway Redmine + MySQL instance for manually exercising
Stride against a real server, and seeds it with demo projects,
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

The built-in `admin` account forces a password change on first login, which can't be done over
the API: open <http://localhost:8080>, log in as `admin` / `admin` and set a new password.

## 4. Seed test data

```bash
pip install -r docker/requirements.txt   # or point PYTHON at a virtualenv, see below
docker/seed.sh --reset
```

`seed.sh` does everything in one go, in about two minutes:

1. `seed_redmine_config.rb` (run inside the container with `rails runner`) does what the REST API
    can't: it enables the REST API, creates the time entry activities and custom fields, and
    reads the admin's API key.
2. `seed_redmine.py` creates users, projects, issues and time entries over the REST API.
3. `seed_redmine_config.rb finish` applies per-project activity settings.

`--reset` deletes the demo projects (and everything in them) first; without it, existing
users/projects are left alone and nothing is added twice. Other users and projects are never
touched. A project left half-seeded by a failed run isn't completed by a later run (the script
warns about it); use `--reset` then. `seed.sh` stops with an error if Redmine's default data
(step 2) hasn't been loaded yet.

Per-project activity settings are listed in `seed_activity_overrides.json`, shared by both
scripts: activities under `inactive` are deactivated in the project (the seeder never books time
on them); activities under `reactivated` are deactivated and activated again, which leaves the
project listing them under a project-specific id that clients must use instead of the shared one.

Further arguments go to `seed_redmine.py`, e.g. `--weeks 8` or `--seed 7` (the default
seed makes runs reproducible). Use `PYTHON=/path/to/venv/bin/python docker/seed.sh` if
`requests` isn't installed system-wide.

### What you get

- **Users** (password `TestPass123!`, API keys printed at the end): `alice`, `bob`, `carol`,
    `dave`, `erin` (Developer role), `frank` (Manager), `grace` (support agent, Developer role).
- **Projects**: Website Relaunch, Mobile App with the subprojects iOS and Android, Internal
    Tools, Customer Support, Data Platform, and Legacy CRM, which is **closed** (read-only - booking
    time on it must fail). Each has members, issue categories and versions (sprints / releases,
    past ones closed).
- **~70 issues** across Bug/Feature/Support with priorities, assignees, start/due dates,
    estimates, done ratios, subtasks, notes from teammates and statuses from New to Closed and
    Rejected.
- **~830 time entries** over the last 6 weeks (workdays, ~35 h per person and week), logged by
    each user themselves, with activities that fit the work, project-level meetings without an
    issue, and occasional overtime, training, travel and on-call weekends.
- **Time entry activities**: Design, Development (default), Testing, Code review,
    Documentation, Meeting, Support, Deployment, Project management, plus the **inactive**
    "Legacy maintenance". **Design is deactivated in Customer Support.** In Internal Tools,
    **Code review** has the project-specific id (booking it with the shared id fails with 422).
- **Custom fields**:
    - time entries: _Special_ (list: Overtime, On-call, Travel, Weekend, Training) and
        _Billable_ (yes/no, default yes)
    - issues: _Customer_ (list), _Environment_ (Production/Staging/Development, on bugs) and
        _Story points_ (on features)

## 5. Point the app at it

In Stride's Settings view, set the Redmine URL to
`http://localhost:8080/` and paste in one of the API keys `seed.sh` printed - a demo user's
rather than the admin's, to test with normal project permissions.
