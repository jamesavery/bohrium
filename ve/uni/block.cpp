/*
This file is part of Bohrium and copyright (c) 2012 the Bohrium
team <http://www.bh107.org>.

Bohrium is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

Bohrium is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the
GNU Lesser General Public License along with Bohrium.

If not, see <http://www.gnu.org/licenses/>.
*/

#include <sstream>
#include <cassert>

#include "block.hpp"
#include "instruction.hpp"

using namespace std;

namespace bohrium {

namespace {
void spaces(stringstream &out, int num) {
    for (int i = 0; i < num; ++i) {
        out << " ";
    }
}

// Returns the views with the greatest number of dimensions
vector<const bh_view*> max_ndim_views(int64_t nviews, const bh_view view_list[]) {
    // Find the max ndim
    int64_t ndim = 0;
    for (int64_t i=0; i < nviews; ++i) {
        const bh_view *view = &view_list[i];
        if (not bh_is_constant(view)) {
            if (view->ndim > ndim)
                ndim = view->ndim;
        }
    }
    vector<const bh_view*> ret;
    for (int64_t i=0; i < nviews; ++i) {
        const bh_view *view = &view_list[i];
        if (not bh_is_constant(view)) {
            if (view->ndim == ndim) {
                ret.push_back(view);
            }
        }
    }
    return ret;
}

// Returns the shape of the view with the greatest number of dimensions
// if equal, the greatest shape is returned
vector<int64_t> dominating_shape(int64_t nviews, const bh_view *view_list) {
    vector<const bh_view*> views = max_ndim_views(nviews, view_list);
    vector<int64_t > shape;
    for(const bh_view *view: views) {
        for (int64_t j=0; j < view->ndim; ++j) {
            if (shape.size() > (size_t)j) {
                if (shape[j] < view->shape[j])
                    shape[j] = view->shape[j];
            } else {
                shape.push_back(view->shape[j]);
            }
        }
    }
    return shape;
}

bool is_reshapeable(const vector<bh_instruction*> &instr_list) {
    assert(instr_list.size() > 0);

    // In order to be reshapeable, all instructions must have the same rank and be reshapeable
    int64_t rank = instr_list[0]->dominating_rank();
    for (auto instr: instr_list) {
        if (not instr->reshapable())
            return false;
        if (instr->dominating_rank() != rank)
            return false;
    }
    return true;
}
} // Anonymous name space

Block create_nested_block(vector<bh_instruction*> &instr_list, set<bh_base *> &news, set<bh_base *> &frees,
                          set<bh_base *> &temps, int rank, int64_t size_of_rank_dim) {
    Block ret;
    ret._news = news;
    ret._frees = frees;
    ret._temps = temps;

    if (instr_list.empty()) {
        throw runtime_error("create_nested_block: 'instr_list' is empty!");
    }

    if (is_reshapeable(instr_list)) {
        for (bh_instruction *instr: instr_list) {
            vector<int64_t> shape((size_t) rank + 1);
            // The dimensions up til 'rank' (not including 'rank') are unchanged
            for (int64_t r=0; r < rank; ++r) {
                shape[r] = instr_list[0]->operand[0].shape[r];
            }

            int64_t size = 1; // The size of the reshapeable block
            for (int64_t r=rank; r < instr->operand[0].ndim; ++r) {
                size *= instr->operand[0].shape[r];
            }
            if (size_of_rank_dim == -1)
                size_of_rank_dim = size;
            assert(size >= size_of_rank_dim);
            shape[rank] = size_of_rank_dim;
            if (size != size_of_rank_dim) {
                if (size % size_of_rank_dim != 0)
                    throw runtime_error("create_nested_block: shape is not divisible with 'size_of_rank_dim'");
                shape.push_back(size / size_of_rank_dim);
            }
            instr->reshape(shape);
        }
        ret._reshapable = true;
    }

    vector<int64_t> shape = dominating_shape(bh_noperands(instr_list[0]->opcode), instr_list[0]->operand);
#ifndef NDEBUG
    // Let's make sure that all (non-system) instructions has the same dominating shape
    for (bh_instruction *instr: instr_list) {
        if (not bh_opcode_is_system(instr->opcode)) {
            int nop = bh_noperands(instr->opcode);
            auto t = dominating_shape(nop, instr->operand);
            assert(t == shape);
        }
    }
#endif
    assert((int)shape.size() > rank);

    // Find the sweeped axes
    vector<set<bh_instruction*> > sweeps(shape.size());
    for (auto instr: instr_list) {
        int axis = sweep_axis(*instr);
        if (axis < BH_MAXDIM) {
            assert(axis < (int)shape.size());
            sweeps[axis].insert(&instr[0]);
        }
    }

    // Let's build the nested block from the 'rank' level to the instruction block
    ret.rank = rank;
    ret.size = shape[rank];
    Block *parent = &ret;
    Block *bottom = &ret;
    ret._sweeps.insert(sweeps[rank].begin(), sweeps[rank].end());
    for(int i=rank+1; i < (int)shape.size(); ++i) {
        Block b;
        b.rank = i;
        b.size = shape[i];
        b._sweeps.insert(sweeps[i].begin(), sweeps[i].end());
        parent->_block_list.push_back(b);
        bottom = &parent->_block_list[0];
        parent = bottom;
    }
    for (bh_instruction *instr: instr_list) {
        Block instr_block;
        instr_block._instr = &instr[0];
        instr_block.rank = (int)shape.size(); // The rank is only to make pretty printing easier
        bottom->_block_list.push_back(instr_block);
    }
    return ret;
}

Block* Block::findInstrBlock(const bh_instruction *instr) {
    if (isInstr()) {
        if (_instr != NULL and _instr == instr)
            return this;
    } else {
        for(Block &b: this->_block_list) {
            Block *ret = b.findInstrBlock(instr);
            if (ret != NULL)
                return ret;
        }
    }
    return NULL;
}

string Block::pprint() const {
    stringstream ss;
    if (isInstr()) {
        if (_instr != NULL) {
            spaces(ss, rank*4);
            ss << *_instr << endl;
        }
    } else {
        spaces(ss, rank*4);
        ss << "rank: " << rank << ", size: " << size;
        if (_sweeps.size() > 0) {
            ss << ", sweeps: { ";
            for (const bh_instruction *i : _sweeps) {
                ss << *i << ",";
            }
            ss << "}";
        }
        if (_reshapable) {
            ss << ", reshapable";
        }
        if (_news.size() > 0) {
            ss << ", news: {";
            for (const bh_base *b : _news) {
                ss << "a" << b->get_label() << ",";
            }
            ss << "}";
        }
        if (_frees.size() > 0) {
            ss << ", frees: {";
            for (const bh_base *b : _frees) {
                ss << "a" << b->get_label() << ",";
            }
            ss << "}";
        }
        if (_temps.size() > 0) {
            ss << ", temps: {";
            for (const bh_base *b : _temps) {
                ss << "a" << b->get_label() << ",";
            }
            ss << "}";
        }
        if (_block_list.size() > 0) {
            ss << ", block list:" << endl;
            for (const Block &b : _block_list) {
                ss << b.pprint();
            }
        }
    }
    return ss.str();
}

void Block::getAllInstr(vector<bh_instruction *> &out) const {
    if (isInstr()) {
        if (_instr != NULL)
            out.push_back(_instr);
    } else {
        for (const Block &b : _block_list) {
            b.getAllInstr(out);
        }
    }
}

vector<bh_instruction *> Block::getAllInstr() const {
    vector<bh_instruction *> ret;
    getAllInstr(ret);
    return ret;
}

Block merge(const Block &a, const Block &b, bool based_on_block_b) {
    assert(not a.isInstr());
    assert(not b.isInstr());
    // For convenient such that the new block always depend on 't1'
    const Block &t1 = based_on_block_b?b:a;
    const Block &t2 = based_on_block_b?a:b;
    Block ret(t1);
    // The block list should always be in order: 'a' before 'b'
    ret._block_list.clear();
    ret._block_list.insert(ret._block_list.end(), a._block_list.begin(), a._block_list.end());
    ret._block_list.insert(ret._block_list.end(), b._block_list.begin(), b._block_list.end());
    // The order of the sets doesn't matter
    ret._sweeps.insert(t2._sweeps.begin(), t2._sweeps.end());
    ret._news.insert(t2._news.begin(), t2._news.end());
    ret._frees.insert(t2._frees.begin(), t2._frees.end());
    std::set_intersection(ret._news.begin(), ret._news.end(), ret._frees.begin(), ret._frees.end(), \
                          std::inserter(ret._temps, ret._temps.begin()));
    ret._reshapable = a._reshapable and b._reshapable;
    return ret;
}

ostream& operator<<(ostream& out, const Block& b) {
    out << b.pprint();
    return out;
}

ostream& operator<<(ostream& out, const vector<Block>& block_list) {
    out << "Block list: " << endl;
    for(const Block &b: block_list) {
        out << b;
    }
    return out;
}

} // bohrium
