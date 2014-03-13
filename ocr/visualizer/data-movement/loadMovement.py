#!/usr/bin/python

def createHeirarchy(filename):
    with open(filename) as file:
        line = file.readline()
        if not (line.startswith("DATAMOVE") and line.find('heirarchy')>=0):
            return None
        counts = []
        for token in line.split():
            if token.isdigit():
                counts.append(int(token))
        board = None
        chips = []
        units = []
        blocks = []
        workers = []
        
        line = file.readline()
        while line.startswith("DATAMOVE INFO"):
            toAdd = dict()
            terms = line.split()
            name = ' '.join(terms[2:4])
            
            toAdd['name'] = name
            toAdd['children'] = []
            toAdd['addr'] = terms[4]
            if terms[2] == 'board':
                if board == None:
                    board = toAdd
                else: return None
            elif terms[2] == 'chip': chips.append(toAdd)
            elif terms[2] == 'unit': units.append(toAdd)
            elif terms[2] == 'block': blocks.append(toAdd)
            elif terms[2] == 'worker': workers.append(toAdd) 
            
            line = file.readline()
        
        # Chips
        for chip in chips:
            board['children'].append(chip)
        for i, unit in enumerate(units):
            chip = i / counts[1]
            board['children'][chip]['children'].append(unit)
        for i, block in enumerate(blocks):
            chip = i / (counts[1] * counts[2])
            unit = i / counts[2] % counts[1]
            board['children'][chip]['children'][unit]['children'].append(block)
        for i, worker in enumerate(workers):
            chip = i / (counts[1] * counts[2] * counts[3])
            unit = i / (counts[3] * counts[2]) % counts[1]
            block = i / counts[3] % counts[2]
            board['children'][chip]['children'][unit]['children'][block]['children'].append(worker)
        
    return board

def makeIndex(heirarchy):
    assert heirarchy.__class__ == dict
    idx = dict()
    idx[heirarchy['addr']] = None
    def addChildren(component):
        print "COMPONENT IS",component
        c = heirarchy
        for i in component:
            c = c['children'][component[i]]
        print c['name']
        idx[c['addr']] = component
        for i,j in enumerate(c['children']):
            addChildren(component+[i])
    for i,j in enumerate(heirarchy['children']):
        addChildren([i])
    return idx

heirarchy = createHeirarchy('datamove.log')
print heirarchy
heirIdx = makeIndex(heirarchy)
print heirIdx
