import xmlrpclib
import time
import xml.etree.ElementTree as ET
import argparse

namedEdt = False

# Create an object to represent our server.
server_url = 'http://127.0.0.1:20738/RPC2'
server = xmlrpclib.Server(server_url)
G = server.ubigraph;
G.clear()
G.set_edge_style_attribute(0, "oriented", "true")

createdColor   = "#123eab"  # blue
runnableColor  = "#fffd00"  # yellow
runningColor   = "#00cc00"  # green
completedColor = "#ff0000"  # red
satisfyColor   = "#123eab"  # blue
depAddColor    = "#00CC00"  # green

Edts = {}
Events = {}
EdgesOnSatisfy = dict()

class Item:
    'This represents a single action that the Ubigraph will perform'
    time = -1
    action = -1
    dst = -1
    src = -1
    name = ""

    def __init__(self,action,dst,time,src,name):
        self.action = action
        self.time = time
        self.dst = dst
        self.src =src
        self.name =name
    def display(self):
        print self.action+ " to " +`self.dst`+ " at " +`self.time`

    def UbiPerform(self):
        global namedEdt
        if self.action == "EdtCreate" :
            G.new_vertex_w_id(self.dst)
            G.set_vertex_attribute(self.dst, "color",createdColor)
        elif self.action == "EdtSched":
            G.set_vertex_attribute(self.dst,"color",runnableColor)
        elif self.action == "EdtStart":
            G.set_vertex_attribute(self.dst,"color",runningColor)
            if namedEdt:
                G.set_vertex_attribute(self.dst, 'label', self.name)
        elif self.action == "EdtEnd":
            G.set_vertex_attribute(self.dst,"color",completedColor)
        elif self.action == "Satisfy":
            edgeID = G.new_edge(self.dst,self.src)
            G.set_edge_attribute(edgeID,"color", satisfyColor)
            curList = EdgesOnSatisfy.get(self.dst, [])
            for e in curList:
                G.set_edge_attribute(e, "color", satisfyColor)
            if len(curList):
                del EdgesOnSatisfy[self.dst]
        elif self.action == "DepAdd":
            edgeID = G.new_edge(self.dst,self.src)
            curList = EdgesOnSatisfy.get(self.dst, [])
            curList.append(edgeID)
            EdgesOnSatisfy[self.dst] = curList
            G.set_edge_attribute(edgeID,"color", depAddColor)
        elif self.action == "EventCreate":
            G.new_vertex_w_id(self.dst)
            G.set_vertex_attribute(self.dst, 'shape', 'torus')
            G.set_vertex_attribute(self.dst, 'color', '#FFFFFF')
        elif self.action != "EdtDestroy" and self.action != "EventDestroy":
            print "Error: unknown command:",self.action
            exit()

def parseXML(args):
    indexer = 0
    tree = ET.parse(args.filename)
    root = tree.getroot()

    items = []
    for child in root:

        #============= get info ===================#
        action = child.tag
        dst = child.attrib["dst"]
        src = child.get("src","0")
        ts = int(child.attrib["time"])
        name = ""
        #============= create index ===================#
        if action == "EdtCreate":
            if dst in Edts.keys():
                print "Error: EDT alread exits"
                exit()
            Edts[dst] = indexer
            indexer += 1
        elif action == "EventCreate":
            if dst in Events.keys():
                print "Error: event alread exits"
                exit()
            Events[dst] = indexer
            indexer += 1

        #============= get index ===================#
        if dst in Edts.keys():
            dstIdx = Edts[dst]
        elif dst in Events.keys():
            dstIdx = Events[dst]
        else:
            print "unknow dst\n", dst
            exit()

        if src in Edts.keys():
            srcIdx = Edts[src]
        elif src in Events.keys():
            srcIdx = Events[src]
        elif src == "0":
            srcIdx = -1
        else:
            print "unknow src\n" , src
            exit()

        #============= destroy index ===================#
        if action == "EdtDestroy":
            if dst not in Edts.keys():
                print "Error: EDT doesnt exits"
                exit()
            Edts.pop(dst)
        if action == "EventDestroy":
            if dst not in Events.keys():
                print "Error: Event does not exist"
                exit()
            Events.pop(dst)

        #elif action == "Satisfy" and dst in Events:
        #    Events.pop(dst)

        if action == "EdtStart":
            name = child.attrib["funcptr"]

        #============= create item ===================#
        items+= [Item(action,dstIdx,ts,srcIdx,name)]
        #print child.tag, child.attrib
    return items;


def main():
    global namedEdt
    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    parser.add_argument("-named", action='store_true', required=False)
    parsedArgs = parser.parse_args()
    print parsedArgs.named
    namedEdt = parsedArgs.named
    print namedEdt

    items = parseXML(parsedArgs)
    currTime = items[0].time - 2;
    # total number of time steps
    step = items[len(items)-1].time - items[0].time
    step = step * 400.0 / len(items)
    print "step =",step
    for item in items:
        # check if the item belong to this step
        if item.time > currTime:
            time.sleep((item.time -  currTime)/ step)
            currTime = item.time
        # display item
        item.UbiPerform()

main();
