#!/usr/bin/env python3
"""Seed a fresh Redmine instance (see docker-redmine.yml) with realistic test data.

Creates a handful of demo users, projects, issues (spread across trackers, statuses
and priorities) and time entries, so QRedmineTimeTracker has something non-trivial to
talk to during manual testing. Safe to re-run: existing users/projects are detected by
login/identifier and left untouched, and issue/time-entry seeding for a project is
skipped once it already has enough of either.

One-time setup (see docker/README.md for the full walkthrough):

    1. docker compose -f docker/docker-redmine.yml up -d
    2. docker compose -f docker/docker-redmine.yml exec -e SECRET_KEY_BASE=supersecretkey redmine \
           rake redmine:load_default_data REDMINE_LANG=en
       (the official Redmine image ships with zero trackers/statuses/roles configured -
       without this, issue/membership creation fails outright)
    3. Open http://localhost:8080, log in as admin/admin, set a new admin password.
    4. Administration -> Settings -> API -> check "Enable REST web service" -> Save.
    5. "My account" (top right) -> API access key -> Show -> copy it.
       (steps 3-5 are manual because none of them are exposed over the REST API itself)

Then:

    pip install -r docker/requirements.txt
    REDMINE_API_KEY=<the key from step 5> python3 docker/seed_redmine.py

Run with --help for all options.
"""

from __future__ import annotations

import argparse
import os
import random
import sys
from dataclasses import dataclass, field
from datetime import date, timedelta
from typing import Any

import requests

DEFAULT_URL = "http://localhost:8080"

DEMO_USERS = [
    {"login": "alice", "firstname": "Alice", "lastname": "Anders", "mail": "alice@example.test"},
    {"login": "bob", "firstname": "Bob", "lastname": "Baker", "mail": "bob@example.test"},
    {"login": "carol", "firstname": "Carol", "lastname": "Clark", "mail": "carol@example.test"},
]
DEMO_USER_PASSWORD = "TestPass123!"

PROJECTS = [
    {
        "identifier": "website-relaunch",
        "name": "Website Relaunch",
        "description": "Redesign and rebuild of the public marketing website.",
    },
    {
        "identifier": "mobile-app",
        "name": "Mobile App",
        "description": "Native mobile application for iOS and Android.",
    },
    {
        "identifier": "internal-tools",
        "name": "Internal Tools",
        "description": "Internal dashboards and automation tooling.",
    },
]

# (tracker name, subject, description) - tracker names are matched against whatever
# trackers actually exist on the server, falling back to the first tracker if a name
# doesn't match (e.g. on a Redmine instance whose default trackers were renamed).
ISSUE_TEMPLATES = [
    ("Bug", "Fix login redirect loop", "Users are redirected back to the login page after a successful sign-in."),
    ("Feature", "Add dark mode toggle", "Provide a user-facing setting to switch between light and dark themes."),
    ("Bug", "Broken pagination on search results", "Clicking 'next page' resets the applied filters."),
    ("Support", "Investigate slow checkout flow", "Customers report the checkout taking >10s on slow connections."),
    ("Feature", "Export report as CSV", "Add a CSV export button to the monthly report view."),
    ("Bug", "Timezone offset incorrect in notifications", "Emails show UTC time instead of the user's local time."),
    ("Feature", "Add two-factor authentication", "Support TOTP-based 2FA as an opt-in security feature."),
    ("Support", "Clarify onboarding documentation", "New users are confused about the initial setup steps."),
    ("Bug", "Memory leak in background sync", "Long-running sessions show steadily increasing memory usage."),
    ("Feature", "Bulk issue assignment", "Allow assigning multiple issues to a user in one action."),
    ("Bug", "Incorrect total on invoice PDF", "Line-item discounts are not reflected in the printed total."),
    ("Feature", "Keyboard shortcuts for common actions", "Add shortcuts for create/save/search."),
]

TIME_ENTRY_COMMENTS = [
    "Investigated root cause",
    "Implemented fix",
    "Wrote tests",
    "Code review feedback",
    "Pair programming session",
    "Reproduced the issue",
    "Deployed to staging",
    "Follow-up after QA feedback",
]


class RedmineApiError(RuntimeError):
    pass


@dataclass
class RedmineSeeder:
    base_url: str
    session: requests.Session
    trackers: list[dict[str, Any]] = field(default_factory=list)
    statuses: list[dict[str, Any]] = field(default_factory=list)
    priorities: list[dict[str, Any]] = field(default_factory=list)
    activities: list[dict[str, Any]] = field(default_factory=list)
    roles: list[dict[str, Any]] = field(default_factory=list)

    def _url(self, path: str) -> str:
        return f"{self.base_url.rstrip('/')}/{path.lstrip('/')}"

    def request(self, method: str, path: str, **kwargs: Any) -> requests.Response:
        response = self.session.request(method, self._url(path), timeout=15, **kwargs)
        if response.status_code >= 400:
            raise RedmineApiError(f"{method} {path} -> {response.status_code}: {response.text[:500]}")
        return response

    def get_json(self, path: str, **kwargs: Any) -> dict[str, Any]:
        response = self.request("GET", path, **kwargs)
        return response.json() if response.content else {}

    def load_reference_data(self) -> None:
        self.trackers = self.get_json("/trackers.json").get("trackers", [])
        self.statuses = self.get_json("/issue_statuses.json").get("issue_statuses", [])
        self.priorities = self.get_json("/enumerations/issue_priorities.json").get("issue_priorities", [])
        self.activities = self.get_json("/enumerations/time_entry_activities.json").get("time_entry_activities", [])
        self.roles = self.get_json("/roles.json").get("roles", [])

    def tracker_id_by_name(self, name: str) -> int:
        for tracker in self.trackers:
            if tracker["name"] == name:
                return tracker["id"]
        return self.trackers[0]["id"]

    def status_id(self, name: str) -> int:
        for status in self.statuses:
            if status["name"] == name:
                return status["id"]
        return self.statuses[0]["id"]

    def random_priority_id(self) -> int:
        return random.choice(self.priorities)["id"]

    def random_activity_id(self) -> int:
        return random.choice(self.activities)["id"]

    def default_role_ids(self) -> list[int]:
        # self.roles only ever contains real, assignable roles (see load_reference_data) - Redmine's
        # two builtin pseudo-roles ("Non member"/"Anonymous") are deliberately excluded from
        # /roles.json and cannot be used as a membership role, so there is no safe non-empty fallback
        # here if a fresh instance genuinely has none configured yet (see add_member).
        return [role["id"] for role in self.roles[:1]]

    def find_project(self, identifier: str) -> dict[str, Any] | None:
        response = self.session.get(self._url(f"/projects/{identifier}.json"), timeout=15)
        if response.status_code == 404:
            return None
        if response.status_code >= 400:
            raise RedmineApiError(f"GET /projects/{identifier}.json -> {response.status_code}: {response.text[:500]}")
        return response.json()["project"]

    def create_project(self, identifier: str, name: str, description: str) -> dict[str, Any]:
        payload = {
            "project": {
                "identifier": identifier,
                "name": name,
                "description": description,
                "is_public": True,
                "enabled_module_names": ["issue_tracking", "time_tracking"],
                "tracker_ids": [tracker["id"] for tracker in self.trackers],
            }
        }
        return self.request("POST", "/projects.json", json=payload).json()["project"]

    def ensure_project_trackers(self, project_id: int) -> None:
        # Idempotent on purpose: also repairs a project that was created (by an earlier run of this
        # script, or otherwise) before `rake redmine:load_default_data` had populated any trackers,
        # which otherwise leaves it with zero enabled trackers and every future issue creation on it
        # failing with a confusing "Project/Tracker/Status cannot be blank" error.
        payload = {"project": {"tracker_ids": [tracker["id"] for tracker in self.trackers]}}
        self.request("PUT", f"/projects/{project_id}.json", json=payload)

    def find_user_by_login(self, login: str) -> dict[str, Any] | None:
        users = self.get_json("/users.json", params={"name": login}).get("users", [])
        for user in users:
            if user["login"] == login:
                return user
        return None

    def create_user(self, login: str, firstname: str, lastname: str, mail: str, password: str) -> dict[str, Any]:
        payload = {
            "user": {
                "login": login,
                "firstname": firstname,
                "lastname": lastname,
                "mail": mail,
                "password": password,
                "must_change_passwd": False,
            }
        }
        return self.request("POST", "/users.json", json=payload).json()["user"]

    def user_api_key(self, user_id: int) -> str | None:
        # Redmine only serializes "api_key" for admins or the user viewing themselves,
        # which holds here since this script authenticates as an admin.
        return self.get_json(f"/users/{user_id}.json").get("user", {}).get("api_key")

    def add_member(self, project_id: int, user_id: int, role_ids: list[int]) -> None:
        if not role_ids:
            return  # nothing assignable exists yet (see default_role_ids); skip rather than fail
        payload = {"membership": {"user_id": user_id, "role_ids": role_ids}}
        try:
            self.request("POST", f"/projects/{project_id}/memberships.json", json=payload)
        except RedmineApiError as error:
            # Already a member from a previous run - not worth treating as fatal.
            if "already" not in str(error).lower():
                raise

    def issue_count(self, project_identifier: str) -> int:
        response = self.get_json("/issues.json", params={"project_id": project_identifier, "status_id": "*", "limit": 1})
        return response.get("total_count", 0)

    def create_issue(self, project_id: int, tracker_id: int, subject: str, description: str, status_id: int,
                      priority_id: int, assigned_to_id: int | None, parent_issue_id: int | None = None) -> dict[str, Any]:
        issue: dict[str, Any] = {
            "project_id": project_id,
            "tracker_id": tracker_id,
            "subject": subject,
            "description": description,
            "status_id": status_id,
            "priority_id": priority_id,
        }
        if assigned_to_id is not None:
            issue["assigned_to_id"] = assigned_to_id
        if parent_issue_id is not None:
            issue["parent_issue_id"] = parent_issue_id
        return self.request("POST", "/issues.json", json={"issue": issue}).json()["issue"]

    def create_time_entry(self, issue_id: int, hours: float, comments: str, spent_on: date, activity_id: int) -> None:
        payload = {
            "time_entry": {
                "issue_id": issue_id,
                "hours": hours,
                "comments": comments,
                "spent_on": spent_on.isoformat(),
                "activity_id": activity_id,
            }
        }
        self.request("POST", "/time_entries.json", json=payload)


def build_session(api_key: str | None, username: str | None, password: str | None) -> requests.Session:
    session = requests.Session()
    session.headers["Content-Type"] = "application/json"
    if api_key:
        session.headers["X-Redmine-API-Key"] = api_key
    elif username and password:
        session.auth = (username, password)
    else:
        raise SystemExit("Provide --api-key (or REDMINE_API_KEY) or --username/--password for authentication.")
    return session


def seed_users(seeder: RedmineSeeder) -> list[dict[str, Any]]:
    created = []
    for user in DEMO_USERS:
        existing = seeder.find_user_by_login(user["login"])
        if existing:
            print(f"  user '{user['login']}' already exists (id={existing['id']}), skipping creation")
            created.append(existing)
            continue
        new_user = seeder.create_user(user["login"], user["firstname"], user["lastname"], user["mail"],
                                       DEMO_USER_PASSWORD)
        print(f"  created user '{user['login']}' (id={new_user['id']})")
        created.append(new_user)
    return created


def seed_projects(seeder: RedmineSeeder, users: list[dict[str, Any]]) -> list[dict[str, Any]]:
    role_ids = seeder.default_role_ids()
    if not role_ids:
        print("  no assignable roles found on the server, demo users will not be added as project members "
              "(run `rake redmine:load_default_data`, or create a role manually, then re-run this script)")
    projects = []
    for spec in PROJECTS:
        project = seeder.find_project(spec["identifier"])
        if project:
            print(f"  project '{spec['identifier']}' already exists (id={project['id']}), skipping creation")
        else:
            project = seeder.create_project(spec["identifier"], spec["name"], spec["description"])
            print(f"  created project '{spec['identifier']}' (id={project['id']})")
        seeder.ensure_project_trackers(project["id"])
        for user in users:
            seeder.add_member(project["id"], user["id"], role_ids)
        projects.append(project)
    return projects


def seed_issues_and_time_entries(seeder: RedmineSeeder, project: dict[str, Any], users: list[dict[str, Any]],
                                  issues_per_project: int) -> list[dict[str, Any]]:
    existing_count = seeder.issue_count(project["identifier"])
    if existing_count >= issues_per_project:
        print(f"  project '{project['identifier']}' already has {existing_count} issues, skipping issue seeding")
        return []

    statuses_by_progress = ["New", "In Progress", "Resolved", "Closed"]
    created_issues = []
    for index in range(issues_per_project):
        tracker_name, subject, description = random.choice(ISSUE_TEMPLATES)
        status_name = random.choices(statuses_by_progress, weights=[4, 3, 2, 1])[0]
        assignee = random.choice(users) if users and random.random() < 0.8 else None
        issue = seeder.create_issue(
            project_id=project["id"],
            tracker_id=seeder.tracker_id_by_name(tracker_name),
            subject=f"{subject} ({project['name']})",
            description=description,
            status_id=seeder.status_id(status_name),
            priority_id=seeder.random_priority_id(),
            assigned_to_id=assignee["id"] if assignee else None,
        )
        created_issues.append(issue)

    print(f"  created {len(created_issues)} issues in '{project['identifier']}'")
    return created_issues


def seed_time_entries(seeder: RedmineSeeder, issues: list[dict[str, Any]]) -> int:
    today = date.today()
    entry_count = 0
    for issue in issues:
        if random.random() < 0.3:
            continue  # leave some issues untouched, like a real backlog would have
        for _ in range(random.randint(1, 3)):
            spent_on = today - timedelta(days=random.randint(0, 13))
            seeder.create_time_entry(
                issue_id=issue["id"],
                hours=round(random.uniform(0.25, 6.0), 2),
                comments=random.choice(TIME_ENTRY_COMMENTS),
                spent_on=spent_on,
                activity_id=seeder.random_activity_id(),
            )
            entry_count += 1
    return entry_count


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default=os.environ.get("REDMINE_URL", DEFAULT_URL), help="Redmine base URL")
    parser.add_argument("--api-key", default=os.environ.get("REDMINE_API_KEY"), help="Admin API key")
    parser.add_argument("--username", default=os.environ.get("REDMINE_USERNAME"), help="Admin username (alternative to --api-key)")
    parser.add_argument("--password", default=os.environ.get("REDMINE_PASSWORD"), help="Admin password (alternative to --api-key)")
    parser.add_argument("--issues-per-project", type=int, default=10, help="Target issue count per seeded project")
    parser.add_argument("--seed", type=int, default=None, help="Random seed, for reproducible test data")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.seed is not None:
        random.seed(args.seed)

    session = build_session(args.api_key, args.username, args.password)
    seeder = RedmineSeeder(base_url=args.url, session=session)

    try:
        current_user = seeder.get_json("/users/current.json")["user"]
    except (RedmineApiError, requests.RequestException) as error:
        print(f"Could not reach Redmine's REST API at {args.url}: {error}", file=sys.stderr)
        print("Make sure the REST API is enabled (Administration -> Settings -> API) and the "
              "credentials are correct. See the module docstring in this file for the full "
              "one-time setup steps.", file=sys.stderr)
        return 1
    print(f"Authenticated as '{current_user['login']}' against {args.url}")

    print("Loading reference data (trackers/statuses/priorities/activities/roles)...")
    seeder.load_reference_data()

    print("Seeding users...")
    users = seed_users(seeder)

    print("Seeding projects...")
    projects = seed_projects(seeder, users)

    print("Seeding issues and time entries...")
    total_issues = 0
    total_time_entries = 0
    for project in projects:
        issues = seed_issues_and_time_entries(seeder, project, users, args.issues_per_project)
        total_issues += len(issues)
        total_time_entries += seed_time_entries(seeder, issues)

    print("\nDone.")
    print(f"  Projects: {', '.join(p['identifier'] for p in projects)}")
    print(f"  New issues created: {total_issues}")
    print(f"  New time entries created: {total_time_entries}")
    print("\nDemo users (password for all: {}):".format(DEMO_USER_PASSWORD))
    for user in users:
        api_key = seeder.user_api_key(user["id"])
        key_hint = api_key if api_key else "(login via web UI to see own API key)"
        print(f"  {user['login']:<8} api_key={key_hint}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
