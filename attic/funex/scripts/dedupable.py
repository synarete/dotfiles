#!/usr/bin/env python3
'''
Deduping oracle: traverse name-space and try to predict possible dedup ratios.

'''

import sys
import os
import stat
import hashlib


class DedupAnalyser:

    '''
        Analyze regular-files trying to predict possible dedup ratio.

        Traverse file-system name-space and analyze regular-files. Uses SHA1
        to define two blocks as equal. Ignores all-zero blocks.
    '''

    BLKSIZE = 8192
    CHNKNBK = 16
    CHNKSIZE = BLKSIZE * CHNKNBK

    def __init__(self) -> None:
        self.objs = {}
        self.hmap = {}
        self.nblks = 0
        self.zdgst = self._blkhash(bytes(self.BLKSIZE))

    def analyze(self, base):
        top = os.path.realpath(base)
        self._analyze_tree(os.path.realpath(top))
        nobjs = len(self.objs)
        nblks = self.nblks
        nbins = len(self.hmap)
        ratio = self._ratio(nblks, nbins)
        return (top, nobjs, nblks, nbins, ratio)

    def _analyze_tree(self, top) -> None:
        for root, dirs, files in os.walk(top):
            for elem in files:
                path = os.path.join(root, elem)
                self._analyze_path(path)

    def _analyze_path(self, path: str) -> None:
        try:
            st = os.lstat(path)
            ino = st.st_ino
            if not stat.S_ISREG(st.st_mode):
                return  # Ignore non-regular files
            if self.objs.get(ino, False):
                return  # Ignore already-traversed files
            with open(path, 'rb') as fh:
                self._analyze_file(fh)
            self.objs[ino] = True
        except (IOError, OSError):
            pass

    def _analyze_file(self, fh) -> None:
        dat = fh.read(self.CHNKSIZE)
        while len(dat) >= self.BLKSIZE:
            self._analyze_chunk(dat)
            dat = bytes(fh.read(self.CHNKSIZE))

    def _analyze_chunk(self, dat):
        nbk = int(len(dat) / self.BLKSIZE)
        for bki in range(0, min(self.CHNKNBK, nbk)):
            beg = bki * self.BLKSIZE
            end = beg + self.BLKSIZE
            blk = dat[beg:end]
            self._analyze_blk(blk)

    def _analyze_blk(self, blk) -> None:
        dgst = self._blkhash(blk)
        if dgst != self.zdgst:  # Ignore all-zero blocks
            self.hmap[dgst] = self.hmap.get(dgst, 0) + 1
            self.nblks += 1

    @staticmethod
    def _ratio(a, b) -> float:
        r = 0.0
        if b > 0:
            r = float(a) / float(b)
        return r

    @staticmethod
    def _blkhash(blk) -> str:
        return hashlib.sha256(blk).hexdigest()


def main():
    for top in sys.argv[1:]:
        (top, nobjs, nblks, nbins, ratio) = DedupAnalyser().analyze(top)
        print("top: {}".format(top))
        print("nobjs: {}".format(nobjs))
        print("nblks: {}".format(nblks))
        print("nbins: {}".format(nbins))
        print("ratio: {0:.4f}".format(ratio))


if __name__ == '__main__':
    main()
