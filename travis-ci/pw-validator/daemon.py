#!/usr/bin/env python3
import json
from github_sync import GithubSync
import os
import time


class PWDaemon(object):
    def __init__(self, cfg):
        with open(cfg) as f:
            self.config = json.load(f)
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
                )
                self.workers.append(worker)

    def loop(self):
        while True:
            for worker in self.workers:
                worker.sync_branches()
            time.sleep(300)


if __name__ == "__main__":
    d = PWDaemon(cfg=f"{os.path.dirname(__file__)}/config.json")
    d.loop()
