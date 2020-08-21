#!/usr/bin/env python3
import json
from github_sync import GithubSync
from github import Github

import os
import time
import sys
import re


class PWDaemon(object):
    def __init__(self, cfg, labels_cfg=None):
        with open(cfg) as f:
            self.config = json.load(f)
        if labels_cfg:
            with open(labels_cfg) as f:
                self.labels_cfg = json.load(f)
        self.workers = []
        for project in self.config.keys():
            for branch in self.config[project]["branches"].keys():
                worker_cfg = self.config[project]["branches"][branch]
                worker = GithubSync(
                    pw_url=worker_cfg["pw_url"],
                    pw_search_patterns=worker_cfg["pw_search_patterns"],
                    pw_lookback=worker_cfg.get("pw_lookback", 7),
                    master=branch,
                    repo_url=project,
                    github_oauth_token=self.config[project]["github_oauth_token"],
                    sync_from=worker_cfg["upstream"],
                    source_master=worker_cfg.get("upstream_branch", "master"),
                    ci_repo=worker_cfg.get("ci_repo", None),
                    ci_branch=worker_cfg.get("ci_branch", None),
                    filter_tags=worker_cfg.get("filter_tags", None),
                )
                self.workers.append(worker)
                if labels_cfg:
                    git = Github(self.config[project]["github_oauth_token"])
                    user = git.get_user()
                    repo = user.get_repo(os.path.basename(project))
                    self.color_labels(repo)

    def color_labels(self, repo):
        all_labels = { x.name.lower(): x for x in repo.get_labels() }
        for l in self.labels_cfg:
            label = l.lower()
            if label in all_labels:
                if (
                    all_labels[label].name != l or
                    all_labels[label].color != self.labels_cfg[l]
                ):
                    all_labels[label].edit(name=l, color=self.labels_cfg[l])
            else:
                repo.create_label(name=l, color=self.labels_cfg[l])

    def loop(self):
        while True:
            for worker in self.workers:
                worker.sync_branches()
            time.sleep(300)


def purge(cfg):
    with open(cfg) as f:
        config = json.load(f)
    for project in config.keys():
        git = Github(config[project]["github_oauth_token"])
        user = git.get_user()
        repo = user.get_repo(os.path.basename(project))
        branches = [x for x in repo.get_branches()]
        for branch_name in branches:
            if re.match(r"series/[0-9]+", branch_name.name):
                repo.get_git_ref(f"heads/{branch_name.name}").delete()


if __name__ == "__main__":
    if "purge" in sys.argv:
        purge(cfg=f"{os.path.dirname(__file__)}/config.json")
    else:
        cfg = f"{os.path.dirname(__file__)}/config.json"
        labels = f"{os.path.dirname(__file__)}/labels.json"
        d = PWDaemon(cfg=cfg, labels_cfg=labels)
        d.loop()
