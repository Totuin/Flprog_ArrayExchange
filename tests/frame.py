#!/usr/bin/env python3
"""Сформировать строку для отправки в Serial Monitor; не открывает COM-порт."""
import sys, binascii
if len(sys.argv) == 3 and sys.argv[1] == '--hex':
    print(sys.argv[2].encode('utf-8').hex().upper())
elif len(sys.argv) == 2:
    command = sys.argv[1].encode('ascii')
    print(f'{sys.argv[1]}*{binascii.crc_hqx(command, 0xffff):04X}')
else:
    print('frame.py "BEGIN;3" или frame.py --hex "Название"')
