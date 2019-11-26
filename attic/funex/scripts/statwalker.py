#!/usr/bin/env python3
import sys
import os
import stat


class StatWalker:

    '''
        Collect and analyze regular-files stat info.

        Traverse file-system name-space and collect+analyze regular-files
        using 'stat' system call.
    '''

    def __init__(self) -> None:
        self.objs = {}

    def analyze(self, base):
        top = os.path.realpath(base)
        self._collect_tree(os.path.realpath(top))
        res = self._process()
        return res

    def _collect_tree(self, top) -> None:
        for root, dirs, files in os.walk(top):
            for elem in files:
                path = os.path.join(root, elem)
                self._collect_path(path)

    def _collect_path(self, path: str) -> None:
        try:
            st = os.lstat(path)
            ino = st.st_ino
            if not stat.S_ISREG(st.st_mode):
                return  # Ignore non-regular files
            if self.objs.get(ino, ()):
                return  # Ignore already-traversed files
            self.objs[ino] = (st.st_size, st.st_blocks)
        except (IOError, OSError):
            pass

    def _process(self):
        sizes = [sz for (sz, bsz) in self.objs.values()]
        sizes.sort()
        average = 0
        median = 0
        less1k = len([sz for sz in sizes if sz < 1024])
        less2k = len([sz for sz in sizes if sz < 2048])
        total = len(sizes)
        if total > 0:
            average = sum(sizes) / float(total)
            median = sizes[int(total / 2)]
        return (total, average, median, less1k, less2k)


def ratio(a, b) -> float:
    r = 0.0
    if b > 0:
        r = float(a) / float(b)
    return r


def main():
    for top in sys.argv[1:]:
        (total, average, median, less1k, less2k) = StatWalker().analyze(top)
        print("top: {}".format(top))
        print("total: {}".format(total))
        print("average: {0:.0f}".format(average))
        print("median: {0:.0f}".format(median))
        print("less-1k: {0} {1:.2f}".format(less1k, ratio(less1k, total)))
        print("less-2k: {0} {1:.2f}".format(less2k, ratio(less2k, total)))


if __name__ == '__main__':
    main()
