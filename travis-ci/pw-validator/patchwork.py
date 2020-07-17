#!/usr/bin/env python3
import json
import requests
import datetime as DT
import re
import logging


class Series(object):
    def __init__(self, data, pw_client):
        """
            Minimal required info to get explicit series is name
        """
        self.pw_client = pw_client
        self._relevant_series = None
        self._diffs = None
        self._tags = None
        self._subject_regexp = re.compile(r"(?P<header>\[[^\]]*\])?(?P<name>.+)")
        for key in data:
            setattr(self, key, data[key])
        self.subject = re.match(self._subject_regexp, data["name"]).group("name")
        self.ignore_tags = re.compile(r"[0-9]+/[0-9]+")
        self.tag_regexp = re.compile(r"^(\[(?P<tags>[^]]*)\])* *((?P<subj>[^: ]+):)")

    @property
    def relevant_series(self):
        """
            cache and return sorted list of relevant series
            where first element is first known version of same subject
            and last is the most recent
        """
        if self._relevant_series:
            return self._relevant_series
        all_series = self.pw_client.get_all(
            "series", filters={"project": self.project["id"], "q": self.subject}
        )
        relevant_series = []
        for s in all_series:
            item = Series(s, self.pw_client)
            # we using full text search which could give ambigous results
            # so we must filter out irrelevant results
            if item.subject == self.subject:
                relevant_series.append(item)
        self._relevant_series = sorted(relevant_series, key=lambda k: k.version)
        return self._relevant_series

    @property
    def diffs(self):
        # fetching patches
        """
            Returns patches preserving original order
        """
        if self._diffs:
            return self._diffs
        self._diffs = []
        for patch in self.patches:
            p = self.pw_client.get("patches", patch["id"])
            self._diffs.append(p)
        return self._diffs

    @property
    def closed(self):
        """
            Series considered closed if at least one patch in this series
            have state accepted
        """
        for diff in self.diffs:
            if diff["state"] == "accepted":
                return True
        return False

    def _parse_for_tags(self, name):
        match = re.match(self.tag_regexp, name)
        if not match:
            return set()
        r = set()
        if match.groupdict()["tags"]:
            tags = match.groupdict()["tags"].split(",")
            for tag in tags:
                if not re.match(self.ignore_tags, tag):
                    r.add(tag)
        if match.groupdict()["subj"]:
            r.add(match.groupdict()["subj"])

        return r

    @property
    def tags(self):
        """
           Tags fetched from series name, diffs and cover letter
        """
        if self._tags:
            return self._tags
        self._tags = set()
        for diff in self.patches:
            self._tags |= self._parse_for_tags(diff["name"])
        if self.cover_letter:
            self._tags |= self._parse_for_tags(self.cover_letter["name"])
        self._tags |= self._parse_for_tags(self.name)
        return self._tags


class Patchwork(object):
    def __init__(self, url, pw_search_patterns, pw_lookback=7):
        self.server = url
        self.logger = logging.getLogger(__name__)

        today = DT.date.today()
        lookback = today - DT.timedelta(days=pw_lookback)
        self.since = lookback.strftime("%Y-%m-%dT%H:%M:%S")
        self.pw_search_patterns = pw_search_patterns

    def _request(self, url):
        self.logger.debug(f"Patchwork {self.server} request: {url}")
        ret = requests.get(url)
        self.logger.debug("Response", ret)
        try:
            self.logger.debug("Response data", ret.json())
        except json.decoder.JSONDecodeError:
            self.logger.debug("Response data", ret.text)

        return ret

    def get(self, object_type, identifier):
        return self._get(f"{object_type}/{identifier}/").json()

    def _get(self, req):
        return self._request(f"{self.server}/api/1.1/{req}")

    def get_all(self, object_type, filters=None):
        if filters is None:
            filters = {}
        params = ""
        for key, val in filters.items():
            if val is not None:
                params += f"{key}={val}&"

        items = []

        response = self._get(f"{object_type}/?{params}")
        # Handle paging, by chasing the "Link" elements
        while response:
            for o in response.json():
                items.append(o)

            if "Link" not in response.headers:
                break

            # There are multiple links separated by commas
            links = response.headers["Link"].split(",")
            # And each link has the format of <url>; rel="type"
            response = None
            for link in links:
                info = link.split(";")
                if info[1].strip() == 'rel="next"':
                    response = self._request(info[0][1:-1])

        return items

    def get_project(self, name):
        all_projects = self.get_all("projects")
        for project in all_projects:
            if project["name"] == name:
                self.logger.debug(f"Found {project}")
                return project

    def get_relevant_subjects(self, full=True):
        subjects = {}
        for pattern in self.pw_search_patterns:
            p = {"since": self.since, "state": 1, "archived": False}
            p.update(pattern)
            print(p)
            all_series = self.get_all("series", filters=p)
            for data in all_series:
                s = Series(data, self)
                if s.subject not in subjects:
                    # we already fetched this subject
                    subjects[s.subject] = s.relevant_series
        return subjects
