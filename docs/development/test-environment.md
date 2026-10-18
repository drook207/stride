# Local Redmine

`docker/` contains a throwaway Redmine + MySQL instance, and a script that
fills it with demo projects, issues, users and time entries. The full
instructions are in
[docker/README.md](https://gitlab.com/drook207/QRedmineTimeTracker/-/blob/main/docker/README.md);
in short:

```bash
# 1. Start Redmine (give it a minute on first start)
docker compose -f docker/docker-redmine.yml up -d

# 2. Load Redmine's default trackers, statuses, roles and workflows
docker compose -f docker/docker-redmine.yml exec -e SECRET_KEY_BASE=supersecretkey redmine \
  rake redmine:load_default_data REDMINE_LANG=en
```

Next, open <http://localhost:8080>, log in as `admin` / `admin` and set a new
password - Redmine forces that on first login. Then seed the demo data:

```bash
pip install -r docker/requirements.txt
docker/seed.sh --reset
```

Finally, point Stride at `http://localhost:8080/` with the API key of one of
the demo users, which `seed.sh` prints at the end.

## What you get

- **Users** `alice`, `bob`, `carol`, `dave`, `erin`, `frank` and `grace`,
    with the password `TestPass123!`.
- **Projects** Website Relaunch, Mobile App (with iOS and Android), Internal
    Tools, Customer Support, Data Platform, and the closed - read-only - Legacy
    CRM.
- About **70 issues** across bugs, features and support requests, and about
    **830 time entries** over the last six weeks.
