#!/usr/bin/env python3
"""Build a Di1610 (KM4/GDI) job from PBM pages. Prototype of the CUPS filter.
usage: km4gen.py OUT --dpi 600|300 --paper a4|letter --tray auto|1|bypass --copies N PAGE.pbm...
"""
import argparse, re, subprocess, os

PBMTOJBG = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../build/jbigkit-2.1/pbmtools/pbmtojbg')
PAPER = {'a4': 0x02, 'letter': 0x0c}          # header byte 30
RES = {600: 0x02, 300: 0x00}                  # header byte 29
TRAY = {'auto': 0x00, '1': 0x01, 'bypass': 0x06}  # header byte 31
SYNC = b'\xff\x05' * 512 + b'\x05\xff' * 512
JOB_MARK = b'\x55' * 6 + b'\xff' * 16 + b'\x55' * 42

def header(dpi, paper, tray, copies, page, w, h):
    return (bytes.fromhex('010525000000') + b'\xff' * 16 + bytes.fromhex('01010205000f03')
            + bytes([RES[dpi], PAPER[paper], TRAY[tray], 0, copies, page, 0, 0, 0])
            + w.to_bytes(2, 'little') + h.to_bytes(2, 'little')
            + b'\x00' * 5 + b'\x03' + b'\x00\x01' + b'\x00' * 14)

def page_block(hdr, bie):
    data = bie + b'\xff\x02'                      # driver appends an empty SDE
    n = (len(data) + 2 + 1023) // 1024            # 1 KiB blocks, last 2 bytes are ff05
    return (hdr + data + b'\x00' * (n * 1024 - 2 - len(data))
            + b'\xff\x05\xff\x05' + n.to_bytes(2, 'big') + b'\x05\xff' * 512 + b'\xff\x05' * 510)

def pbm_size(path):
    m = re.match(rb'P4\s+(?:#.*\s+)*(\d+)\s+(\d+)', open(path, 'rb').read(64))
    return int(m[1]), int(m[2])

def main():
    a = argparse.ArgumentParser()
    a.add_argument('out'); a.add_argument('pages', nargs='+')
    a.add_argument('--dpi', type=int, default=600); a.add_argument('--paper', default='a4')
    a.add_argument('--tray', default='auto'); a.add_argument('--copies', type=int, default=1)
    o = a.parse_args()
    out = SYNC + JOB_MARK
    for i, p in enumerate(o.pages, 1):
        w, h = pbm_size(p)
        bie = subprocess.run([PBMTOJBG, '-q', '-o', '3', '-p', '72', '-m', '0', '-s', str(h), p],
                             capture_output=True, check=True).stdout
        out += page_block(header(o.dpi, o.paper, o.tray, o.copies, i, w, h), bie)
    open(o.out, 'wb').write(out + JOB_MARK)

if __name__ == '__main__':
    main()
