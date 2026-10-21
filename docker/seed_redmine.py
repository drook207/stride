#!/usr/bin/env python3
"""Seed the local Redmine test environment (see docker-redmine.yml) with realistic data.

Creates a small software agency's worth of data for end-to-end testing Stride:

- 7 users with different roles (developers, QA, a project manager, a support agent)
- 7 projects, including two subprojects and a closed (read-only) project, each with members,
  issue categories and versions (sprints / releases, past ones closed)
- realistic issues per project: trackers, priorities, assignees, start/due dates, estimates,
  parent/child tasks, notes from teammates, and the issue custom fields Customer, Environment
  and Story points; statuses are set at the end like in a real project (resolved, closed,
  rejected, ...)
- several weeks of time entries on workdays, logged *by* each user (X-Redmine-Switch-User), with
  activities that fit the work, the time entry custom fields Special and Billable, a few
  project-level meetings without an issue, and occasional overtime / on-call weekends

Time entry activities and custom fields can't be created over the REST API; they come from
docker/seed_redmine_config.rb, which docker/seed.sh runs before this script. See
docker/README.md.

    pip install -r docker/requirements.txt
    REDMINE_API_KEY=<admin key> python3 docker/seed_redmine.py [--reset]

Without --reset, existing users and projects are detected and left alone; only projects that don't
exist yet are seeded. A project left half-seeded by a failed run is therefore not completed by a
later run - the script warns about it, and --reset recovers. With --reset, the seeded projects (and
everything in them) are deleted first; other projects and users are never touched.

Activities a project deactivates are listed in seed_activity_overrides.json, which
seed_redmine_config.rb applies to the server and this script uses to never book time on them.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import sys
import time
from dataclasses import dataclass, field
from datetime import date, timedelta
from typing import Any

import requests

DEFAULT_URL = "http://localhost:8080"
# Per-project activity overrides, shared with seed_redmine_config.rb.
ACTIVITY_OVERRIDES = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                 "seed_activity_overrides.json")))
DEMO_USER_PASSWORD = "TestPass123!"

# login, first name, last name, role in the projects they're a member of
USERS = [
    ("alice", "Alice", "Anders", "Developer"),  # frontend
    ("bob", "Bob", "Baker", "Developer"),  # backend, on-call
    ("carol", "Carol", "Clark", "Developer"),  # mobile
    ("dave", "Dave", "Dawson", "Developer"),  # data
    ("erin", "Erin", "Evans", "Developer"),  # QA
    ("frank", "Frank", "Fischer", "Manager"),  # project manager
    ("grace", "Grace", "Green", "Developer"),  # support agent, on-call
]

# Issue templates: (tracker, subject, description[, children]). Children are subtasks
# (tracker, subject) created under the parent.
PROJECTS: list[dict[str, Any]] = [
    {
        "identifier": "website-relaunch",
        "name": "Website Relaunch",
        "description": "Redesign and rebuild of ACME Corp's public marketing website.",
        "customer": "ACME Corp",
        "members": ["alice", "bob", "erin", "frank"],
        "categories": ["Frontend", "Backend", "Content"],
        "versions": [("Sprint 41", -21, True), ("Sprint 42", -7, True), ("Sprint 43", 7, False), ("Sprint 44", 21, False)],
        "issues": [
            ("Feature", "New navigation with mega menu", "Replace the flat top navigation with a mega menu for the product pages.", [
                ("Feature", "Mega menu: desktop layout"),
                ("Feature", "Mega menu: mobile drawer"),
                ("Bug", "Mega menu: keyboard focus gets lost"),
            ]),
            ("Bug", "Fix login redirect loop", "Users are redirected back to the login page after signing in when cookies are blocked."),
            ("Feature", "Newsletter signup with double opt-in", "Signup form in the footer, double opt-in mail via the CRM."),
            ("Bug", "Broken pagination on search results", "Clicking 'next page' resets the applied filters."),
            ("Feature", "Cookie consent banner", "GDPR compliant consent banner, scripts only load after consent."),
            ("Bug", "Hero image is blurry on retina displays", "The hero image is served at 1x resolution only."),
            ("Feature", "Product comparison table", "Compare up to three products side by side."),
            ("Support", "Clarify content migration plan with ACME marketing", "Agree which legacy pages are migrated, rewritten or dropped."),
            ("Bug", "Contact form sends duplicate emails", "Double-clicking submit sends the form twice."),
            ("Feature", "Lighthouse performance budget in CI", "Fail the build when the performance score drops below 90."),
            ("Bug", "Footer links point to the staging site", ""),
            ("Feature", "Blog listing with tag filter", "Filter blog posts by tag without a page reload."),
            ("Feature", "Accessibility audit fixes", "Fix the findings of the external WCAG 2.2 AA audit."),
        ],
    },
    {
        "identifier": "mobile-app",
        "name": "Mobile App",
        "description": "Globex Corporation's field service app for iOS and Android.",
        "customer": "Globex Corporation",
        "members": ["carol", "bob", "erin", "frank"],
        "categories": ["API", "UI", "Release"],
        "versions": [("1.4.0", -18, True), ("1.5.0", 10, False), ("2.0.0", 45, False)],
        "issues": [
            ("Feature", "Offline mode for work orders", "Work orders must be viewable and editable without connectivity.", [
                ("Feature", "Local storage for work orders"),
                ("Feature", "Sync queue with conflict detection"),
                ("Bug", "Sync queue drops edits after app restart"),
            ]),
            ("Bug", "App crashes when opening a work order without customer", "Null customer on legacy work orders."),
            ("Feature", "Push notifications for new assignments", ""),
            ("Bug", "Timezone offset incorrect in notifications", "Notifications show UTC instead of the technician's local time."),
            ("Feature", "Photo attachments with compression", "Attach photos to a work order, compressed to max. 1 MB."),
            ("Bug", "Login button unresponsive on small screens", ""),
            ("Feature", "Signature capture on completion", "Customer signs on the device when a job is completed."),
            ("Support", "Prepare release notes for 1.5.0", ""),
            ("Bug", "Memory leak in background sync", "Long-running sessions show steadily increasing memory usage."),
            ("Feature", "Two-factor authentication", "TOTP based 2FA, opt-in per user."),
        ],
    },
    {
        "identifier": "mobile-app-ios",
        "name": "iOS",
        "parent": "mobile-app",
        "description": "iOS specific work for the Mobile App.",
        "customer": "Globex Corporation",
        "members": ["carol", "erin", "frank"],
        "categories": [],
        "versions": [],
        "issues": [
            ("Bug", "Keyboard covers the notes field on iPhone SE", ""),
            ("Feature", "Support for iOS 26 widgets", "Show today's assignments in a home screen widget."),
            ("Bug", "Face ID prompt shown twice after update", ""),
            ("Support", "App Store review: missing privacy manifest entry", ""),
        ],
    },
    {
        "identifier": "mobile-app-android",
        "name": "Android",
        "parent": "mobile-app",
        "description": "Android specific work for the Mobile App.",
        "customer": "Globex Corporation",
        "members": ["carol", "bob", "erin", "frank"],
        "categories": [],
        "versions": [],
        "issues": [
            ("Bug", "Back button closes the app on the work order screen", ""),
            ("Feature", "Material You theming", "Use dynamic colors on Android 12+."),
            ("Bug", "Crash on Samsung devices when taking a photo", "Only on One UI 7, camera intent returns null."),
            ("Feature", "Target SDK 36", "Required for Play Store updates from November."),
        ],
    },
    {
        "identifier": "internal-tools",
        "name": "Internal Tools",
        "description": "Internal dashboards and automation tooling.",
        "customer": "Internal",
        "members": ["alice", "bob", "dave", "frank", "grace"],
        "categories": ["Tooling", "Infrastructure"],
        "versions": [],
        "issues": [
            ("Feature", "Time tracking dashboard per customer", "Monthly hours per customer and project, exportable as CSV.", [
                ("Feature", "Dashboard: aggregate hours per customer"),
                ("Feature", "Dashboard: CSV export"),
            ]),
            ("Bug", "Nightly backup job fails on full disk", ""),
            ("Feature", "Slack notifications for failed deployments", ""),
            ("Support", "Onboard new developer laptops", "Set up dev environments for two new hires."),
            ("Feature", "Upgrade CI runners to Ubuntu 26.04", ""),
            ("Bug", "VPN drops connection every 30 minutes", ""),
            ("Feature", "Self-service password reset", ""),
            ("Support", "Clarify onboarding documentation", "New colleagues are confused about the initial setup steps."),
        ],
    },
    {
        "identifier": "customer-support",
        "name": "Customer Support",
        "description": "Second level support for all customers. Design is not bookable here.",
        "customer": None,  # set per issue
        "members": ["grace", "erin", "bob", "frank"],
        "categories": ["Account", "Billing", "Technical"],
        "versions": [],
        "issues": [
            ("Support", "Customer cannot reset password", ""),
            ("Support", "Invoice shows wrong VAT rate", ""),
            ("Bug", "Incorrect total on invoice PDF", "Line-item discounts are not reflected in the printed total."),
            ("Support", "Request: export of all work orders 2025", ""),
            ("Support", "Login not possible after email change", ""),
            ("Bug", "Report generation times out for large accounts", ""),
            ("Support", "Question about data retention period", ""),
            ("Support", "Duplicate user accounts after SSO migration", ""),
            ("Support", "Bulk issue assignment not working", ""),
            ("Support", "Setup of a new branch office", ""),
        ],
    },
    {
        "identifier": "data-platform",
        "name": "Data Platform",
        "description": "Reporting and analytics platform for Umbrella Health.",
        "customer": "Umbrella Health",
        "members": ["dave", "bob", "frank"],
        "categories": ["Ingestion", "Reporting"],
        "versions": [("Milestone 1", -14, True), ("Milestone 2", 28, False)],
        "issues": [
            ("Feature", "Ingest lab results via HL7 FHIR", "", [
                ("Feature", "FHIR client with OAuth2"),
                ("Feature", "Map Observation resources to the warehouse schema"),
                ("Bug", "Duplicate observations on re-ingest"),
            ]),
            ("Feature", "Pseudonymization of patient identifiers", ""),
            ("Bug", "Weekly report shows last week's numbers twice", ""),
            ("Feature", "Export report as CSV", "Add a CSV export to the monthly report view."),
            ("Feature", "Row-level access per clinic", ""),
            ("Bug", "Dashboard slow for date ranges over a year", ""),
            ("Support", "Data protection impact assessment input", ""),
        ],
    },
    {
        "identifier": "legacy-crm",
        "name": "Legacy CRM",
        "description": "Maintenance of Initech's old CRM until its shutdown. Closed: read-only.",
        "customer": "Initech",
        "members": ["bob", "dave", "frank"],
        "categories": [],
        "versions": [],
        "closed": True,
        "issues": [
            ("Bug", "Customer search returns deleted contacts", ""),
            ("Support", "Export all contacts for the migration", ""),
            ("Bug", "PDF quotes have wrong letterhead", ""),
            ("Support", "Shutdown checklist", ""),
        ],
    },
]

# Final status distribution (applied after time was logged), and the done ratio that goes with it.
FINAL_STATUSES = [("New", 3), ("In Progress", 5), ("Feedback", 1), ("Resolved", 2), ("Closed", 3), ("Rejected", 1)]
DONE_RATIO = {"New": (0, 0), "In Progress": (10, 80), "Feedback": (50, 90), "Resolved": (90, 100),
              "Closed": (100, 100), "Rejected": (0, 0)}

# Activities that fit an issue's tracker, with weights.
ACTIVITIES_BY_TRACKER = {
    "Bug": [("Development", 6), ("Testing", 3), ("Code review", 1), ("Deployment", 1)],
    "Feature": [("Development", 6), ("Design", 2), ("Code review", 2), ("Testing", 2), ("Documentation", 1)],
    "Support": [("Support", 6), ("Meeting", 1), ("Documentation", 1)],
}
# Activities of a project manager, regardless of the tracker.
MANAGER_ACTIVITIES = [("Project management", 5), ("Meeting", 3), ("Documentation", 1)]

COMMENTS = {
    "Development": ["Implemented the change", "Refactored the affected module", "Fixed edge cases",
                    "Investigated root cause", "Pair programming session", "Worked in review feedback"],
    "Testing": ["Wrote regression tests", "Manual test on staging", "Reproduced the issue", "Verified the fix"],
    "Code review": ["Reviewed merge request", "Second review after changes"],
    "Design": ["Wireframes", "Mockups for review", "Adjusted design after feedback"],
    "Documentation": ["Updated the documentation", "Wrote release notes", "Updated the runbook"],
    "Deployment": ["Deployed to staging", "Production deployment", "Rollback and redeploy"],
    "Support": ["Answered customer request", "Call with the customer", "Analyzed logs for the customer",
                "Follow-up with the customer"],
    "Meeting": ["Alignment with the customer", "Technical discussion", "Estimation"],
    "Project management": ["Sprint planning", "Status report", "Backlog grooming", "Budget review"],
}
PROJECT_MEETINGS = ["Daily stand-up", "Sprint planning", "Retrospective", "Weekly status call"]
NOTES = ["Can reproduce this on staging.", "Waiting for feedback from the customer.",
         "Merge request is ready for review.", "Deployed to staging, please verify.",
         "This needs a decision from the product owner.", "Fixed in the latest build."]


class RedmineApiError(RuntimeError):
    pass


@dataclass
class Redmine:
    base_url: str
    session: requests.Session

    def request(self, method: str, path: str, as_user: str | None = None, **kwargs: Any) -> requests.Response:
        headers = {"X-Redmine-Switch-User": as_user} if as_user else {}
        url = f"{self.base_url.rstrip('/')}/{path.lstrip('/')}"
        response = self.session.request(method, url, headers=headers, timeout=30, **kwargs)
        if response.status_code >= 400:
            raise RedmineApiError(f"{method} {path} -> {response.status_code}: {response.text[:500]}")
        return response

    def get(self, path: str, **kwargs: Any) -> dict[str, Any]:
        response = self.request("GET", path, **kwargs)
        return response.json() if response.content else {}

    def find(self, path: str) -> dict[str, Any] | None:
        """GETs a resource; None if it doesn't exist (404), raises on any other error."""
        url = f"{self.base_url.rstrip('/')}/{path.lstrip('/')}"
        response = self.session.get(url, timeout=30)
        if response.status_code == 404:
            return None
        if response.status_code != 200:
            raise RedmineApiError(f"GET {path} -> {response.status_code}: {response.text[:500]}")
        return response.json()


@dataclass
class Reference:
    trackers: dict[str, int] = field(default_factory=dict)
    statuses: dict[str, int] = field(default_factory=dict)
    priorities: list[dict[str, Any]] = field(default_factory=list)
    activities: dict[str, int] = field(default_factory=dict)
    roles: dict[str, int] = field(default_factory=dict)
    custom_fields: dict[str, int] = field(default_factory=dict)

    @classmethod
    def load(cls, redmine: Redmine) -> "Reference":
        ref = cls()
        ref.trackers = {t["name"]: t["id"] for t in redmine.get("/trackers.json")["trackers"]}
        ref.statuses = {s["name"]: s["id"] for s in redmine.get("/issue_statuses.json")["issue_statuses"]}
        ref.priorities = redmine.get("/enumerations/issue_priorities.json")["issue_priorities"]
        ref.activities = {a["name"]: a["id"]
                          for a in redmine.get("/enumerations/time_entry_activities.json")["time_entry_activities"]
                          if a.get("active", True)}
        ref.roles = {r["name"]: r["id"] for r in redmine.get("/roles.json")["roles"]}
        ref.custom_fields = {c["name"]: c["id"] for c in redmine.get("/custom_fields.json")["custom_fields"]}
        missing = [name for name in ("Special", "Billable", "Customer", "Environment", "Story points")
                   if name not in ref.custom_fields]
        if missing or "Testing" not in ref.activities:
            raise SystemExit(f"Custom fields {missing or ''} / activities missing - run docker/seed_redmine_config.rb "
                             "setup first (docker/seed.sh does both).")
        return ref

    def priority(self) -> int:
        names = [p["name"] for p in self.priorities]
        weights = [{"Low": 2, "Normal": 6, "High": 3, "Urgent": 1, "Immediate": 0.3}.get(n, 1) for n in names]
        return self.priorities[random.choices(range(len(names)), weights)[0]]["id"]


def weighted(choices: list[tuple[str, float]]) -> str:
    return random.choices([c[0] for c in choices], [c[1] for c in choices])[0]


def quarter_hours(value: float) -> float:
    return max(0.25, round(value * 4) / 4)


# ---------------------------------------------------------------------------------------------
# Users and projects


def seed_users(redmine: Redmine) -> dict[str, dict[str, Any]]:
    users = {}
    for login, first, last, _ in USERS:
        found = [u for u in redmine.get("/users.json", params={"name": login})["users"] if u["login"] == login]
        if found:
            users[login] = found[0]
            print(f"  user '{login}' exists")
            continue
        payload = {"user": {"login": login, "firstname": first, "lastname": last, "mail": f"{login}@example.test",
                            "password": DEMO_USER_PASSWORD, "must_change_passwd": False}}
        users[login] = redmine.request("POST", "/users.json", json=payload).json()["user"]
        print(f"  created user '{login}'")
    return users


def delete_projects(redmine: Redmine) -> None:
    # Children first is not needed (deleting a parent deletes its subprojects), but deleting them
    # explicitly keeps this working if they were ever moved.
    for spec in reversed(PROJECTS):
        identifier = spec["identifier"]
        if redmine.find(f"/projects/{identifier}.json"):
            redmine.request("DELETE", f"/projects/{identifier}.json")
            print(f"  deleted project '{identifier}'")
    # Recent Redmine versions delete projects in a background job; wait until they're gone, so their
    # identifiers can be reused.
    for spec in PROJECTS:
        for _ in range(60):
            if not redmine.find(f"/projects/{spec['identifier']}.json"):
                break
            time.sleep(1)
        else:
            raise SystemExit(f"Project '{spec['identifier']}' still exists after deleting it")


def seed_project(redmine: Redmine, ref: Reference, spec: dict[str, Any], users: dict[str, dict[str, Any]],
                 project_ids: dict[str, int]) -> tuple[dict[str, Any], bool]:
    identifier = spec["identifier"]
    existing = redmine.find(f"/projects/{identifier}.json")
    if existing:
        project = existing["project"]
        issues = redmine.get("/issues.json", params={"project_id": project["id"], "status_id": "*", "limit": 1})
        if issues.get("total_count", 0) == 0:
            print(f"  WARNING: project '{identifier}' exists but has no issues (an earlier run failed?); "
                  "it is not seeded again - re-run with --reset")
        else:
            print(f"  project '{identifier}' exists")
        return project, False

    payload = {"project": {"identifier": identifier, "name": spec["name"], "description": spec["description"],
                           "is_public": True, "enabled_module_names": ["issue_tracking", "time_tracking"],
                           "tracker_ids": list(ref.trackers.values()),
                           "issue_custom_field_ids": [ref.custom_fields[n] for n in ("Customer", "Environment",
                                                                                      "Story points")]}}
    if spec.get("parent"):
        payload["project"]["parent_id"] = project_ids[spec["parent"]]
        payload["project"]["inherit_members"] = False
    project = redmine.request("POST", "/projects.json", json=payload).json()["project"]
    print(f"  created project '{identifier}'")

    roles = {login: role for login, _, _, role in USERS}
    for login in spec["members"]:
        redmine.request("POST", f"/projects/{project['id']}/memberships.json",
                        json={"membership": {"user_id": users[login]["id"], "role_ids": [ref.roles[roles[login]]]}})
    for name in spec["categories"]:
        redmine.request("POST", f"/projects/{project['id']}/issue_categories.json",
                        json={"issue_category": {"name": name}})
    today = date.today()
    for name, due_in_days, _ in spec["versions"]:
        redmine.request("POST", f"/projects/{project['id']}/versions.json",
                        json={"version": {"name": name, "due_date": (today + timedelta(days=due_in_days)).isoformat()}})
    return project, True


# ---------------------------------------------------------------------------------------------
# Issues


@dataclass
class SeededIssue:
    id: int
    project: str
    tracker: str
    assignee: str | None
    start_date: date
    parent_id: int | None = None
    final_status: str = "New"


def custom_field_values(ref: Reference, spec: dict[str, Any], tracker: str) -> list[dict[str, Any]]:
    customer = spec["customer"] or random.choice(["ACME Corp", "Globex Corporation", "Initech", "Umbrella Health"])
    values = [{"id": ref.custom_fields["Customer"], "value": customer}]
    if tracker == "Bug":
        values.append({"id": ref.custom_fields["Environment"],
                       "value": weighted([("Production", 5), ("Staging", 3), ("Development", 1)])})
    if tracker == "Feature":
        values.append({"id": ref.custom_fields["Story points"], "value": str(random.choice([1, 2, 3, 5, 8, 13]))})
    return values


def seed_issues(redmine: Redmine, ref: Reference, spec: dict[str, Any], project: dict[str, Any],
                history_days: int, user_ids: dict[str, int]) -> list[SeededIssue]:
    today = date.today()
    categories = {c["name"]: c["id"] for c in
                  redmine.get(f"/projects/{project['id']}/issue_categories.json")["issue_categories"]}
    developers = [m for m in spec["members"] if m != "frank"]
    seeded: list[SeededIssue] = []

    def create(tracker: str, subject: str, description: str, parent_id: int | None = None) -> SeededIssue:
        author = random.choice(spec["members"])
        assignee = random.choice(developers) if random.random() < 0.85 else None
        # Legacy CRM work happened in the weeks before it was closed, everything else during the
        # seeded history (see seed_time_entries()).
        if spec.get("closed"):
            start = today - timedelta(days=random.randint(history_days + 42, history_days + 70))
        else:
            start = today - timedelta(days=random.randint(history_days // 2, history_days + 14))
        issue: dict[str, Any] = {"project_id": project["id"], "tracker_id": ref.trackers[tracker], "subject": subject,
                                 "description": description, "priority_id": ref.priority(),
                                 "start_date": start.isoformat(),
                                 "custom_fields": custom_field_values(ref, spec, tracker)}
        if random.random() < 0.6:
            issue["due_date"] = (start + timedelta(days=random.randint(7, 40))).isoformat()
        if random.random() < 0.7:
            issue["estimated_hours"] = random.choice([1, 2, 3, 4, 6, 8, 12, 16, 24])
        if categories:
            issue["category_id"] = random.choice(list(categories.values()))
        if parent_id:
            issue["parent_issue_id"] = parent_id
        if assignee:
            issue["assigned_to_id"] = user_ids[assignee]
        created = redmine.request("POST", "/issues.json", as_user=author, json={"issue": issue}).json()["issue"]
        result = SeededIssue(created["id"], spec["identifier"], tracker, assignee, start, parent_id)
        seeded.append(result)
        return result

    for template in spec["issues"]:
        tracker, subject, description = template[0], template[1], template[2]
        parent = create(tracker, subject, description)
        for child_tracker, child_subject in (template[3] if len(template) > 3 else []):
            create(child_tracker, child_subject, "", parent_id=parent.id)

    # Notes from teammates on some issues.
    for issue in seeded:
        if random.random() < 0.3:
            redmine.request("PUT", f"/issues/{issue.id}.json", as_user=random.choice(spec["members"]),
                            json={"issue": {"notes": random.choice(NOTES)}})
    print(f"  created {len(seeded)} issues in '{spec['identifier']}'")
    return seeded


def finish_issues(redmine: Redmine, ref: Reference, spec: dict[str, Any], project: dict[str, Any],
                  issues: list[SeededIssue]) -> None:
    """Sets the final statuses, done ratios and target versions, then closes past versions."""
    versions = {v["name"]: v["id"] for v in redmine.get(f"/projects/{project['id']}/versions.json")["versions"]}
    closed_statuses = ("Closed", "Rejected")
    # Subtasks first: Redmine doesn't let an issue with open subtasks be closed, and silently drops
    # such a status change from an API update.
    for issue in sorted(issues, key=lambda i: i.parent_id is None):
        status = weighted(FINAL_STATUSES) if not spec.get("closed") else weighted([("Closed", 4), ("Rejected", 1)])
        children = [i for i in issues if i.parent_id == issue.id]
        if status in closed_statuses and any(c.final_status not in closed_statuses for c in children):
            status = "Resolved"
        issue.final_status = status
        low, high = DONE_RATIO[status]
        update: dict[str, Any] = {"status_id": ref.statuses[status], "done_ratio": random.randint(low, high) // 10 * 10}
        if versions:
            past = [name for name, _, closed in spec["versions"] if closed]
            future = [name for name, _, closed in spec["versions"] if not closed]
            pool = past if status in ("Closed", "Resolved") and past else future or past
            update["fixed_version_id"] = versions[random.choice(pool)]
        redmine.request("PUT", f"/issues/{issue.id}.json", json={"issue": update})
        actual = redmine.get(f"/issues/{issue.id}.json")["issue"]["status"]["name"]
        if actual != status:
            print(f"  WARNING: issue #{issue.id} is '{actual}', Redmine did not accept '{status}'")
    for name, _, closed in spec["versions"]:
        if closed:
            redmine.request("PUT", f"/versions/{versions[name]}.json", json={"version": {"status": "closed"}})


# ---------------------------------------------------------------------------------------------
# Time entries


def entry_custom_fields(ref: Reference, spec: dict[str, Any], activity: str, special: str | None) -> list[dict[str, Any]]:
    billable = spec["customer"] != "Internal" and activity not in ("Meeting", "Project management")
    values = [{"id": ref.custom_fields["Billable"], "value": "1" if billable else "0"}]
    if special:
        values.append({"id": ref.custom_fields["Special"], "value": special})
    return values


def seed_time_entries(redmine: Redmine, ref: Reference, issues: list[SeededIssue], history_days: int,
                      legacy_days: tuple[int, int], project_ids: dict[str, int]) -> int:
    today = date.today()
    specs = {spec["identifier"]: spec for spec in PROJECTS}
    count = 0

    def log(user: str, day: date, hours: float, issue: SeededIssue | None, project: str, activity: str,
            comment: str, special: str | None = None) -> None:
        nonlocal count
        entry: dict[str, Any] = {"hours": hours, "spent_on": day.isoformat(), "comments": comment,
                                 "activity_id": ref.activities[activity],
                                 "custom_fields": entry_custom_fields(ref, specs[project], activity, special)}
        if issue:
            entry["issue_id"] = issue.id
        else:
            entry["project_id"] = project_ids[project]  # the REST API wants the numeric id here
        redmine.request("POST", "/time_entries.json", as_user=user, json={"time_entry": entry})
        count += 1

    def activity_for(user: str, project: str, tracker: str) -> str:
        choices = MANAGER_ACTIVITIES if user == "frank" else ACTIVITIES_BY_TRACKER[tracker]
        inactive = ACTIVITY_OVERRIDES.get(project, {}).get("inactive", [])
        return weighted([c for c in choices if c[0] not in inactive])

    def work_day(user: str, day: date, pool: list[SeededIssue], target: float, special: str | None = None) -> None:
        mine = [i for i in pool if i.assignee == user] or pool
        remaining = target
        while remaining >= 0.25 and pool:
            # Mostly own issues, sometimes helping out on others.
            issue = random.choice(mine if random.random() < 0.75 else pool)
            hours = quarter_hours(min(remaining, random.uniform(0.5, 4)))
            activity = activity_for(user, issue.project, issue.tracker)
            log(user, day, hours, issue, issue.project, activity, random.choice(COMMENTS[activity]), special)
            remaining -= hours

    # Only projects whose issues were just created get time entries (including the project-level
    # meetings), so re-running the script without --reset doesn't add any to existing projects.
    seeded = {issue.project for issue in issues}
    member_of = {login: [s["identifier"] for s in PROJECTS
                         if login in s["members"] and not s.get("closed") and s["identifier"] in seeded]
                 for login, *_ in USERS}

    # Regular weeks
    for offset in range(history_days, -1, -1):
        day = today - timedelta(days=offset)
        for login, *_ in USERS:
            if day.weekday() >= 5:
                # On-call weekends for backend and support.
                if login in ("bob", "grace") and random.random() < 0.15:
                    pool = [i for i in issues if i.project in member_of[login] and i.start_date <= day]
                    work_day(login, day, pool, quarter_hours(random.uniform(1, 3)), "On-call")
                continue
            if random.random() < 0.06:
                continue  # vacation / sick day
            pool = [i for i in issues if i.project in member_of[login] and i.start_date <= day]
            target = quarter_hours(random.uniform(6, 8))
            special = None
            if random.random() < 0.05:
                target += quarter_hours(random.uniform(1, 2.5))
                special = "Overtime"
            elif random.random() < 0.03:
                special = "Training"
            elif login == "frank" and random.random() < 0.05:
                special = "Travel"
            # Stand-up and sprint meetings logged on the project, without an issue.
            if day.weekday() in (0, 4) and random.random() < 0.6 and member_of[login]:
                project = random.choice(member_of[login])
                hours = 0.25 if day.weekday() == 4 else 1.0
                log(login, day, hours, None, project, "Meeting", random.choice(PROJECT_MEETINGS))
                target -= hours
            work_day(login, day, pool, target, special)

    # Legacy CRM: the weeks before it was closed.
    legacy = [i for i in issues if i.project == "legacy-crm"]
    for offset in range(legacy_days[1], legacy_days[0] - 1, -1):
        day = today - timedelta(days=offset)
        if day.weekday() >= 5:
            continue
        for login in specs["legacy-crm"]["members"]:
            if random.random() < 0.5:
                work_day(login, day, legacy, quarter_hours(random.uniform(0.5, 3)))
    return count


# ---------------------------------------------------------------------------------------------




def build_session(api_key: str | None, username: str | None, password: str | None) -> requests.Session:
    session = requests.Session()
    session.headers["Content-Type"] = "application/json"
    if api_key:
        session.headers["X-Redmine-API-Key"] = api_key
    elif username and password:
        session.auth = (username, password)
    else:
        raise SystemExit("Provide --api-key (or REDMINE_API_KEY) or --username/--password of an admin.")
    return session


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default=os.environ.get("REDMINE_URL", DEFAULT_URL), help="Redmine base URL")
    parser.add_argument("--api-key", default=os.environ.get("REDMINE_API_KEY"), help="Admin API key")
    parser.add_argument("--username", default=os.environ.get("REDMINE_USERNAME"), help="Admin username")
    parser.add_argument("--password", default=os.environ.get("REDMINE_PASSWORD"), help="Admin password")
    parser.add_argument("--weeks", type=int, default=6, help="Weeks of time entries to create (default: 6)")
    parser.add_argument("--seed", type=int, default=42, help="Random seed for reproducible data (default: 42)")
    parser.add_argument("--reset", action="store_true",
                        help="Delete the seeded projects (and all their issues and time entries) first")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    random.seed(args.seed)
    redmine = Redmine(args.url, build_session(args.api_key, args.username, args.password))

    try:
        current = redmine.get("/users/current.json")["user"]
    except (RedmineApiError, requests.RequestException) as error:
        print(f"Could not reach Redmine's REST API at {args.url}: {error}", file=sys.stderr)
        print("Run docker/seed.sh, or see docker/README.md for the setup steps.", file=sys.stderr)
        return 1
    if not current.get("admin"):
        print(f"'{current['login']}' is not an admin; seeding needs an admin API key.", file=sys.stderr)
        return 1
    print(f"Authenticated as '{current['login']}' against {args.url}")

    ref = Reference.load(redmine)

    if args.reset:
        print("Deleting seeded projects...")
        delete_projects(redmine)

    print("Users...")
    users = seed_users(redmine)
    user_ids = {login: user["id"] for login, user in users.items()}

    history_days = args.weeks * 7
    print("Projects...")
    project_ids: dict[str, int] = {}
    new_projects = []
    for spec in PROJECTS:
        project, created = seed_project(redmine, ref, spec, users, project_ids)
        project_ids[spec["identifier"]] = project["id"]
        if created:
            new_projects.append((spec, project))

    print("Issues...")
    all_issues: list[SeededIssue] = []
    issues_by_project: dict[str, list[SeededIssue]] = {}
    for spec, project in new_projects:
        issues = seed_issues(redmine, ref, spec, project, history_days, user_ids)
        issues_by_project[spec["identifier"]] = issues
        all_issues.extend(issues)

    print("Time entries (this takes a minute)...")
    entries = seed_time_entries(redmine, ref, all_issues, history_days, (history_days + 14, history_days + 42),
                                project_ids)
    print(f"  created {entries} time entries")

    print("Final issue states, versions and closed projects...")
    for spec, project in new_projects:
        finish_issues(redmine, ref, spec, project, issues_by_project[spec["identifier"]])
        if spec.get("closed"):
            redmine.request("PUT", f"/projects/{project['id']}/close.json")
            print(f"  closed project '{spec['identifier']}'")

    print("\nDone.")
    print(f"  Projects: {', '.join(spec['identifier'] for spec in PROJECTS)}")
    print(f"  New issues: {len(all_issues)}, new time entries: {entries}")
    print(f"\nUsers (password for all: {DEMO_USER_PASSWORD}):")
    for login, _, _, role in USERS:
        key = redmine.get(f"/users/{users[login]['id']}.json").get("user", {}).get("api_key")
        print(f"  {login:6} {role:10} API key: {key}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
