from patchwork import Patchwork
import concurrent.futures

import atexit

import random
import os
import shutil
import logging
import subprocess
import gc

VMTEST = "travis-ci/vmtest/"
GENERIC_VMTEST = "test-generic"
PREPARE_ST = "prepare_selftests.sh"
KERNEL_DIR = "bpf-next"


@atexit.register
def cleanup():
    for obj in gc.get_objects():
        if isinstance(obj, Validator):
            for f in obj.cleanup:
                # just print pls
                print("rm -rf %s" % f)


class Validator(object):
    def __init__(self, concurrency=5):
        self.logger = logging.getLogger(__name__)
        self.pw = Patchwork(
            "https://patchwork.ozlabs.org/", "tsipa740", "DERPDERPDERP", 7
        )
        # "DERPDERPDERP" API TOKEN
        self.concurrency = concurrency
        self.executor = concurrent.futures.ThreadPoolExecutor(self.concurrency)
        self.cleanup = []
        self.cwd = os.getcwd()

    def popen(self, cmd, env):
        current_env = os.environ.copy()
        current_env.update(env)
        self.logger.warning("Running %s with env %s", " ".join(cmd), env)
        call = subprocess.Popen(
            cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=current_env
        )
        out, err = call.communicate()
        log = self.logger.debug
        if call.returncode != 0:
            log = self.logger.warn
        log("Out: %s Error: %s", out, err)
        return call.returncode

    def initial_setup(self):
        """ checkout empty unpatched tree only once """
        shutil.copytree(VMTEST, GENERIC_VMTEST)
        self.cleanup.append(GENERIC_VMTEST)
        env = {"KERNEL": "LATEST", "VMTEST_ROOT": f"{self.cwd}/{GENERIC_VMTEST}"}
        self.popen(
            [f"{GENERIC_VMTEST}/{PREPARE_ST}", f"{GENERIC_VMTEST}/{KERNEL_DIR}"],
            env=env,
        )

    def validate_series(self, series_id, patches):
        """
            Run tests for single set for one series id:
            create test dir, put patches and run test script
        """
        self.logger.warn("Running %s patches: %s", series_id, len(patches))
        td = f"test-{series_id}"
        ptd = f"test-{series_id}/patches"
        cwd = os.getcwd()
        shutil.copytree(VMTEST, td)
        os.mkdir(ptd)
        self.cleanup.append(td)
        n = 1
        for patch in patches:
            f = open(f"{ptd}/{n}.patch", "w")
            f.write(patch)
            f.close()
            n += 1

        env = {"KERNEL": "LATEST", "VMTEST_ROOT": f"{self.cwd}/{td}"}
        self.popen([f"{td}/{PREPARE_ST}", f"{td}/{KERNEL_DIR}"], env=env)

        self.logger.warn("Done %s", series_id)

    def run_validation(self):
        """ Run all tests ~concurrently against all relevant series and get results """
        tasks = []
        self.initial_setup()
        for series in self.pw.get_relevant_series():
            task = self.executor.submit(self.validate_series, series[0], series[1])
            tasks.append(task)
            # break
        while len(tasks) > 0:
            completed, pending = concurrent.futures.wait(
                tasks, timeout=None, return_when=concurrent.futures.FIRST_COMPLETED
            )
            for t in completed:
                tasks.remove(t)


v = Validator()
v.run_validation()
