#!/usr/bin/env python3
from patchwork import Patchwork, Series
from github import Github
import git
import re
import tempfile
import shutil
import os
import logging
import hashlib


class GithubSync(object):
    def __init__(
        self,
        pw_url,
        pw_search_patterns,
        master,
        repo_url,
        github_oauth_token,
        sync_from,
        source_master,
        ci_repo=None,
        ci_branch=None,
        merge_conflict_label="merge conflict",
        pw_lookback=7,
        filter_tags=None,
    ):
        self.ci_repo = ci_repo
        if self.ci_repo:
            self.ci_branch = ci_branch
            self.ci_repo_dir = self._uniq_tmp_folder(ci_repo, ci_branch)
        self.repo_name = os.path.basename(repo_url)
        self.repo_url = repo_url
        self.sync_from = sync_from
        self.master = master
        self.source_master = source_master
        self.git = Github(github_oauth_token)
        self.pw = Patchwork(pw_url, pw_search_patterns, pw_lookback=pw_lookback, filter_tags=filter_tags)
        self.user = self.git.get_user()
        self.user_login = self.user.login
        self.local_repo = self.user.get_repo(self.repo_name)
        self.repo = self.user.get_repo(self.repo_name)
        self.branches = [x for x in self.repo.get_branches()]
        self.merge_conflict_label = merge_conflict_label
        # self.master = self.repo.get_branch(master)
        self.logger = logging.getLogger(__name__)
        # self.repodir = tempfile.TemporaryDirectory()
        self.repodir = self._uniq_tmp_folder(repo_url, master)
        self.subjects = {}
        self.prs = {}
        self.all_prs = {}
        self.pr_branch_regexp = re.compile("series/[0-9]+")
        self.fetch_master()
        self.get_pulls()
        self.do_sync()

    def _uniq_tmp_folder(self, url, branch):
        # use same foder for multiple invocation to avoid cloning whole tree every time
        # but use different folder for different workers identified by url and branch name
        sha = hashlib.sha256()
        sha.update(f"{url}/{branch}".encode("utf-8"))
        return f"/tmp/pw_sync_{sha.hexdigest()}"

    def do_sync(self):
        if "sync_from" in self.local_repo.remotes:
            urls = list(self.local_repo.remote("sync_from").urls)
            if urls != [self.sync_from]:
                self.logger.warn(f"remote sync_from set to track {urls}, re-creating")
                self.local_repo.delete_remote("sync_from")
                self.local_repo.create_remote("sync_from", self.sync_from)
        else:
            self.local_repo.create_remote("sync_from", self.sync_from)
        self.source = self.local_repo.remote("sync_from")
        self.source.fetch(self.source_master)
        self.source_branch = getattr(self.source.refs, self.source_master)
        self._reset_repo()
        self.local_repo.git.push(
            "-f", "origin", f"{self.source_branch}:refs/heads/{self.master}"
        )
        self.master_sha = self.source_branch.object.hexsha

    def fetch_master(self):
        """
            Fetch master only once
        """
        if os.path.exists(f"{self.repodir}/.git"):
            self.local_repo = git.Repo.init(self.repodir)
            self.local_repo.git.fetch("-p", self.repo_url)
        else:
            shutil.rmtree(self.repodir, ignore_errors=True)
            self.local_repo = git.Repo.clone_from(self.repo_url, self.repodir)
        if self.ci_repo:
            if os.path.exists(f"{self.ci_repo_dir}/.git"):
                self.ci_local_repo = git.Repo.init(self.ci_repo_dir)
                self.ci_local_repo.git.fetch("-p", self.ci_repo)
            else:
                shutil.rmtree(self.ci_repo_dir, ignore_errors=True)
                self.ci_local_repo = git.Repo.clone_from(self.ci_repo, self.ci_repo_dir)
            self.ci_local_repo.git.checkout(f"origin/{self.ci_branch}")

    def _reset_repo(self):
        self.local_repo.git.reset("--hard", self.source_branch)
        self.source_branch.checkout()

    def _create_dummy_commit(self, branch_name):
        """
            Reset branch, create dummy commit
        """
        self._reset_repo()
        if branch_name in self.local_repo.branches:
            self.local_repo.git.branch("-D", branch_name)
        self.local_repo.git.checkout("-b", branch_name)
        self.local_repo.git.commit("--allow-empty", "-m", "Dummy commit")
        self.local_repo.git.push("-f", "origin", branch_name)

    def _flag_pr(self, pr):
        """
            PRs with merge conflicts should not receive spam about merge conflicts
        """
        pr.add_to_labels(self.merge_conflict_label)

    def _unflag_pr(self, pr):
        pr.remove_from_labels(self.merge_conflict_label)

    def _is_pr_flagged(self, pr):
        for label in pr.get_labels():
            if self.merge_conflict_label == label.name:
                return True
        return False

    def _close_pr(self, pr):
        pr.edit(state="closed")

    def _comment_series_pr(
        self,
        series,
        message,
        tags=None,
        branch_name=None,
        can_create=False,
        close=False,
        flag=False,
    ):
        """
            Appends comment to a PR.
        """
        title = f"{series.subject}"
        if title in self.prs:
            pr = self.prs[title]
        elif can_create:
            if flag:
                self._create_dummy_commit(branch_name)
            body = (
                f"Pull request for series with\nsubject: {series.subject}\n"
                f"version: {series.version}\n"
                f"url: {series.web_url}\n"
            )
            pr = self.repo.create_pull(
                title=title, body=body, head=branch_name, base=self.master
            )
            self.prs[title] = pr
        else:
            return False

        if pr.state == "closed" and close:
            # If PR already closed do nothing
            return pr

        if not flag and self._is_pr_flagged(pr):
            # remove flag and comment
            self._unflag_pr(pr)
            pr.create_issue_comment(message)

        if flag and not self._is_pr_flagged(pr):
            # set flag and comment
            self._flag_pr(pr)
            pr.create_issue_comment(message)
        for tag in series.tags:
            pr.add_to_labels(tag)

        if close:
            self._close_pr(pr)
        return pr

    def checkout_and_patch(self, branch_name, series_to_apply):
        """
            Patch in place and push.
            Returns true if whole series applied.
            Return False if at least one patch in series failed.
            If at least one patch in series failed nothing gets pushed.
        """
        self._reset_repo()
        if branch_name in self.local_repo.branches:
            self.local_repo.git.branch("-D", branch_name)
        self.local_repo.git.checkout("-b", branch_name)
        # import pdb; pdb.set_trace()
        if series_to_apply.closed:
            comment = f"At least one diff in series {series_to_apply.web_url} have state 'accepted'. Closing PR."
            self._comment_series_pr(series_to_apply, comment, close=True)
            # delete branch if there is no more PRs left from this branch
            if (
                branch_name in self.all_prs
                and len(self.all_prs[branch_name]) == 1
                and branch_name in self.branches
            ):
                self.repo.get_git_ref(f"heads/{branch_name}").delete()
            return False
        diffs = series_to_apply.diffs
        fname = None
        fname_commit = None
        title = f"{series_to_apply.subject}"
        comment = (
            f"Master branch: {self.master_sha}\nseries: {series_to_apply.web_url}\n"
            f"version: {series_to_apply.version}\n"
        )
        # TODO: omg this is damn ugly
        if self.ci_repo:
            os.system(
                f"cp -a {self.ci_repo_dir}/* {self.ci_repo_dir}/.travis.yml {self.repodir}"
            )
            self.local_repo.git.add("-A")
            self.local_repo.git.add("-f", ".travis.yml")
            self.local_repo.git.commit("-a", "-m", "adding ci files")
        for diff in diffs:
            f = tempfile.NamedTemporaryFile(mode="w", delete=False)
            f.write(diff["diff"])
            fname = f.name
            f.close()
            try:
                self.local_repo.git.apply([fname])
                author = diff["submitter"]["name"]
                email = diff["submitter"]["email"]
                content = diff["content"]
                self.local_repo.git.add("-A")
                f = tempfile.NamedTemporaryFile(mode="w", delete=False)
                f.write(content)
                fname_commit = f.name
                f.close()
                self.local_repo.git.commit(
                    f"--author='{author} <{email}>'", "-F", f"{fname_commit}"
                )
                self.logger.warn(f'{branch_name} / {diff["id"]} applied successfully"')
                comment = f"{comment}\npatch {diff['web_url']} applied successfully"
            except git.exc.GitCommandError as e:
                comment = f"{comment}\nPull request is *NOT* updated. Failed to apply {diff['web_url']}, error message was:\n{e}"
                self.logger.warn(
                    f'Failed to apply patch {diff["id"]} on top of {branch_name} for series {series_to_apply.id}'
                )

                self._comment_series_pr(
                    series_to_apply,
                    comment,
                    can_create=True,
                    branch_name=branch_name,
                    flag=True,
                )
                os.unlink(fname)
                if fname_commit:
                    os.unlink(fname_commit)
                succeed = False
                return False
            os.unlink(fname)
            if fname_commit:
                os.unlink(fname_commit)

        # force push only if if's a new branch or there is code diffs between old and new braches
        # which could mean that we applied new set of patches or just rebased
        if (
            branch_name not in self.local_repo.remotes.origin.refs
            or self.local_repo.git.diff(branch_name, f"remotes/origin/{branch_name}")
        ):
            self.local_repo.git.push("-f", "origin", branch_name)
            self._comment_series_pr(
                series_to_apply, comment, can_create=True, branch_name=branch_name
            )
        return True

    def get_pulls(self):
        for pr in self.repo.get_pulls():
            if self._is_relevant_pr(pr):
                self.prs[pr.title] = pr

            self.all_prs.setdefault(pr.head.ref, {}).setdefault(pr.base.ref, [])
            if pr.state == "open":
                self.all_prs[pr.head.ref][pr.base.ref].append(pr)

    def _is_relevant_pr(self, pr):
        """
            PR is relevant if it
            - coming from user
            - to same user
            - coming from branch with pattern series/[0-9]+
            - to branch {master}
            - is open
        """
        src_user = pr.head.user.login
        src_branch = pr.head.ref
        tgt_user = pr.base.user.login
        tgt_branch = pr.base.ref
        state = pr.state
        if (
            src_user == self.user_login
            and re.match(self.pr_branch_regexp, src_branch)
            and tgt_user == self.user_login
            and tgt_branch == self.master
            and state == "open"
        ):
            return True
        return False

    def sync_branches(self):
        """
            One subject = one branch
            creates branches when necessary
            apply patches where it's necessary
            delete branches where it's necessary
            version of series applies in the same branch
            as separate commit
        """
        self.subjects = self.pw.get_relevant_subjects()
        # fetch recent subjects
        for subject_name in self.subjects:
            subject = self.subjects[subject_name]
            series_id = subject[0].id
            # branch name == sid of the first known series
            branch_name = f"series/{series_id}"
            # series to apply - last known series
            series = subject[-1]
            self.checkout_and_patch(branch_name, series)
        # sync old subjects
        for subject_name in self.prs:
            pr = self.prs[subject_name]
            if subject_name not in self.subjects and self._is_relevant_pr(pr):
                branch_name = self.prs[subject_name].head.label.split(":")[1]
                series_id = branch_name.split("/")[1]
                series = Series(self.pw.get("series", series_id), self.pw)
                subject = series.relevant_series
                branch_name = f"series/{series_id}"
                self.checkout_and_patch(branch_name, subject[-1])
