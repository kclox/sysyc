import graphviz
import json
import sys

g = graphviz.Digraph()
g_id = 0


def NodeId():
    global g_id
    g_id = g_id + 1
    return str(g_id - 1)


def CreateSubGraph(d, parent):
    if isinstance(d, dict):
        for k, v in d.items():
            id = NodeId()
            g.node(id, label=k)
            g.edge(parent, id)
            CreateSubGraph(v, id)
    elif isinstance(d, list):
        for i, it in enumerate(d):
            id = NodeId()
            g.node(id, label=str(i))
            g.edge(parent, id)
            CreateSubGraph(it, id)
    else:
        id = NodeId()
        g.node(id, label=str(d))
        g.edge(parent, id)


if __name__ == "__main__":
    if len(sys.argv) == 2:
        jf_name = sys.argv[1]
        with open(jf_name) as jf:
            d = json.load(jf)
            mid = NodeId()
            g.node(mid, label="Module")
            CreateSubGraph(d, mid)
            print(g)
