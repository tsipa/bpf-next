#!/usr/bin/env python3

import argparse
from github import Github, GithubException
import os
import sys


class GH(object):
    def get_repo(self, git, project):
        repo_name = os.path.basename(project)
        try:
            user = git.get_user()
            repo = user.get_repo(repo_name)
        except GithubException:
            org = os.path.split(project)[0].split(":")[-1]
            repo = git.get_organization(org).get_repo(repo_name)
        return repo

    def __init__(self):
        for k in ["GH_TOKEN", "TRAVIS_REPO_SLUG", "TRAVIS_PULL_REQUEST"]:
            attr = os.getenv(k)
            if not attr:
                raise RuntimeError(f"{k} variable is not set")
            setattr(self, k, attr)
        self.git = Github(self.GH_TOKEN)
        self.current_repo = self.get_repo(self.git, self.TRAVIS_REPO_SLUG)
        self.current_pr = self.current_repo.get_pull(int(self.TRAVIS_PULL_REQUEST))

    def comment(self, text):
        self.current_pr.create_issue_comment(text)


def parse_args():
    parser = argparse.ArgumentParser(description="Github helper for travis CI")
    parser.add_argument(
        "action", choices=["comment"], help="Append stdin as a comment to PR"
    )
    args = parser.parse_args()
    return args


args = parse_args()
if args.action == "comment":
    g = GH()
    text = []
    for line in sys.stdin:
        text.append(line)

    text = "\n".join(text)

    g.comment(text)
