#!/usr/bin/python
import sqlite3
import re
from compiler.ast import TryExcept

MACHINE_TABLE = 'layout'
TABLE_NAME = 'movement'

def quote(s):
    return '"' + s + '"'

def map_from_file(filename):
    components = dict()
    with open(filename) as f:
        line = ''
        while not(line.startswith("DATAMOVE hierarchy")):
            line = f.readline()
        counts = []
        for token in line.split():
            if token.isdigit():
                counts.append(int(token))
        for line in f.readlines():
            if not line.startswith("DATAMOVE INFO"): continue
            terms = line.split()
            if terms[2] == 'board':
                components[terms[4]] = 'b'
            elif terms[2] == 'chip':
                components[terms[4]] = terms[3]
            elif terms[2] == 'unit':
                chip = str(int(terms[3]) / counts[1])
                unit = str(int(terms[3]) % counts[1]) 
                components[terms[4]] = '.'.join([chip, unit])
            elif terms[2] == 'block':
                chip = str(int(terms[3]) / (counts[2] * counts[1]))
                unit = str(int(terms[3]) / counts[2] % counts[1])
                block = str(int(terms[3]) % counts[2])
                components[terms[4]] = '.'.join([chip, unit, block])
            elif terms[2] == 'worker':
                chip = str(int(terms[3]) / (counts[3] * counts[2] * counts[1]))
                unit = str(int(terms[3]) / (counts[3] * counts[2]) % counts[1])
                block = str(int(terms[3]) / counts[3] % counts[2])
                worker = str(int(terms[3]) % counts[3])
                components[terms[4]] = '.'.join([chip, unit, block, worker])
    return components

def check_move_line(line):
    return re.match("DATAMOVE: \w+ \d+ bytes 0x\w+ -> 0x\w+$", line) != None

def parse_file(filename, outfile):
    print 'Initializing database...'
    conn = sqlite3.connect(outfile)
    c = conn.cursor()
    components = map_from_file(filename)
    c.execute("DROP TABLE IF EXISTS %s" % MACHINE_TABLE)
    c.execute('''CREATE TABLE %s(
                 id TEXT PRIMARY KEY,
                 parent TEXT)''' % MACHINE_TABLE)
    print components
    for component in components.values():
        try:
            parent = component[:component.rindex('.')]
        except ValueError:
            parent = 'b'
            if component == parent:
                parent = '.' # To indicate null
        s = "INSERT INTO %s(id, parent) VALUES(%s,%s)" % (MACHINE_TABLE, quote(component), quote(parent))
        print s
        c.execute(s)
    c.execute("DROP TABLE IF EXISTS %s" % TABLE_NAME)
    c.execute('''CREATE TABLE %s(
                 source TEXT,
                 destination TEXT,
                 edt TEXT,
                 size REAL)''' % TABLE_NAME)
    for line in open(filename):
        if not check_move_line(line): continue
        tokens = line.split()
        edt = quote(tokens[1])
        size = int(tokens[2])
        source = quote(components[tokens[4]])
        dest = quote(components[tokens[6]])
        insert = 'INSERT INTO %s(source, destination, edt, size) VALUES(%s,%s,%s,%s)' % (
                  TABLE_NAME, source, dest, edt, size)
        c.execute(insert)
    conn.commit()
    c.close()

parse_file('datamove.log', 'testing.db')
