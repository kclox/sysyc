#include "ir/ir.hpp"
#include "opt/opt.h"
#include "json.hpp"
#include <queue>
#include <stack>
#include <cstdio>
#include <cstring>

namespace opt {

using GVT = std::shared_ptr<ir::Value>;

struct BBInfo : ir::BB::BBExtraInfo {
    int id;
    bool vis;
    dtree::Node node;

    BBInfo(int id = 0, bool vis = false) : id(id), vis(vis) {}
};

static int bbinfo_id = -1;

inline BBInfo *bbinfo(ir::BB *bb) {
    assert(bbinfo_id == 0);
    return bb->extra[bbinfo_id]->cast<BBInfo>();
}

dtree::Node &dtree::GetNode(ir::BB *bb) { return bbinfo(bb)->node; }

ir::BB *dtree::IDom(ir::BB *bb) { return GetNode(bb).parent; }

bool dbg_dfs = false;
bool dbg_dtree = false;

struct DTreeBuilder {
  private:
    std::vector<std::unique_ptr<ir::BB>> &bblocks;
    std::vector<ir::BB *> id_to_bb;
    int n;
    std::vector<std::set<int>> preds;
    std::vector<int> doms;

    void dfs(ir::BB *bb) {
        auto bi = bbinfo(bb);
        if (bi->vis)
            return;
        bi->vis = true;
        for (auto &succ : bb->successors) {
            dfs(succ);
        }
        bi->id = n;
        id_to_bb[n] = bb;
        n++;
    }

    int intersect(int finger1, int finger2) {
        while (finger1 != finger2) {
            while (finger1 < finger2)
                finger1 = doms[finger1];
            while (finger2 < finger1)
                finger2 = doms[finger2];
        }
        return finger1;
    }

    void set_preds(ir::BB *bb) {
        auto bi = bbinfo(bb);
        if (!bi->vis)
            return;
        bi->vis = false;
        for (auto s : bb->successors) {
            preds[bbinfo(s)->id].insert(bi->id);
            set_preds(s);
            if (dbg_dfs)
                fprintf(stderr, "%d -> %d\n", bi->id, bbinfo(s)->id);
        }
    }

  public:
    DTreeBuilder(std::vector<std::unique_ptr<ir::BB>> &bblocks)
        : bblocks(bblocks) {
        n = 0;
        int i = 0;
        for (auto &bb : bblocks) {
            if (i == 0)
                bbinfo_id = bb->extra.size();
            bb->id = i++;
            bb->extra.push_back(std::make_unique<BBInfo>());
        }
        int size = bblocks.size();
        id_to_bb.reserve(size);
        preds.resize(size);
        doms.resize(size);
        std::fill_n(doms.begin(), size, -1);
    }

    void Build() {
        dfs(bblocks[0].get());
        if (dbg_dfs)
            fprintf(stderr, "digraph {\n label=\"dfs\"\n");
        set_preds(bblocks[0].get());
        if (dbg_dfs)
            fprintf(stderr, "}\n");

        doms[n - 1] = n - 1;

        bool changed = true;
        while (changed) {
            changed = false;
            for (int b = n - 2; b >= 0; b--) {
                int new_idom;
                bool fisrt = true;
                for (auto it = preds[b].rbegin(); it != preds[b].rend(); it++) {
                    auto p = *it;
                    if (fisrt) {
                        new_idom = p;
                        fisrt = false;
                    } else {
                        if (doms[p] != -1) {
                            new_idom = intersect(p, new_idom);
                        }
                    }
                }
                if (doms[b] != new_idom) {
                    doms[b] = new_idom;
                    changed = true;
                }
            }
        }

        for (int i = 0; i < n; i++) {
            auto &to = id_to_bb[i];
            auto from = id_to_bb[doms[i]];
            if (from != to) {
                bbinfo(to)->node.parent = from;
                bbinfo(from)->node.children.push_back(to);
            }
        }

        if (dbg_dtree) {
            fprintf(stderr, "digraph {\n label=\"dtree\"\n");
            for (int i = 0; i < n; i++) {
                fprintf(stderr, "%d -> %d\n", doms[i], i);
            }
            fprintf(stderr, "}\n");
        }
    }
};

void BuildDTree(ir::Module &m) {
    for (auto &func : m.funcs) {
        DTreeBuilder builder(func->bblocks);
        builder.Build();
    }
}

} // namespace opt