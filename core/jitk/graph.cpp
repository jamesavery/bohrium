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

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/foreach.hpp>
#include <fstream>
#include <numeric>
#include <queue>

#include <jitk/graph.hpp>
#include <jitk/block.hpp>

using namespace std;

namespace bohrium {
namespace jitk {
namespace graph {

/* Determines whether there exist a path from 'a' to 'b'
 *
 * Complexity: O(E + V)
 *
 * @a               The first vertex
 * @b               The second vertex
 * @dag             The DAG
 * @only_long_path  Only accept path of length greater than one
 * @return          True if there is a path
 */
bool path_exist(Vertex a, Vertex b, const DAG &dag, bool only_long_path) {
    using namespace boost;

    struct path_visitor:default_bfs_visitor {
        const Vertex dst;
        path_visitor(Vertex b):dst(b){};

        void examine_edge(Edge e, const DAG &g) const {
            if(target(e,g) == dst)
                throw runtime_error("");
        }
    };
    struct long_visitor:default_bfs_visitor {
        const Vertex src, dst;
        long_visitor(Vertex a, Vertex b):src(a),dst(b){};

        void examine_edge(Edge e, const DAG &g) const
        {
            if(source(e,g) != src and target(e,g) == dst)
                throw runtime_error("");
        }
    };
    try {
        if(only_long_path)
            breadth_first_search(dag, a, visitor(long_visitor(a,b)));
        else
            breadth_first_search(dag, a, visitor(path_visitor(b)));
    }
    catch (const runtime_error &e) {
        return true;
    }
    return false;
}

// Create a DAG based on the 'block_list'
DAG from_block_list(const vector<Block> &block_list) {
    DAG graph;
    map<bh_base*, set<Vertex> > base2vertices;
    for (const Block &block: block_list) {
        assert(block.validation());
        Vertex vertex = boost::add_vertex(&block, graph);

        // Find all vertices that must connect to 'vertex'
        // using and updating 'base2vertices'
        set<Vertex> connecting_vertices;
        for (bh_base *base: block.getAllBases()) {
            set<Vertex> &vs = base2vertices[base];
            connecting_vertices.insert(vs.begin(), vs.end());
            vs.insert(vertex);
        }

        // Finally, let's add edges to 'vertex'
        BOOST_REVERSE_FOREACH (Vertex v, connecting_vertices) {
            if (vertex != v and block.depend_on(*graph[v])) {
                boost::add_edge(v, vertex, graph);
            }
        }
    }
    return graph;
}

uint64_t weight(const Block &a, const Block &b) {
    const set<bh_base *> news = a.getAllNews();
    const set<bh_base *> frees = b.getAllFrees();
    vector<bh_base *> new_temps;
    set_intersection(news.begin(), news.end(), frees.begin(), frees.end(), back_inserter(new_temps));

    uint64_t totalsize = 0;
    for (const bh_base *base: new_temps) {
        totalsize += bh_base_size(base);
    }
    return totalsize;
}

uint64_t block_cost(const Block &block) {
    std::vector<bh_base*> non_temps;
    const set<bh_base *> temps = block.getAllTemps();
    for (const bh_instruction *instr: block.getAllInstr()) {
        // Find non-temporary arrays
        const int nop = bh_noperands(instr->opcode);
        for (int i = 0; i < nop; ++i) {
            const bh_view &v = instr->operand[i];
            if (not bh_is_constant(&v) and temps.find(v.base) == temps.end()) {
                if (std::find(non_temps.begin(), non_temps.end(), v.base) == non_temps.end()) {
                    non_temps.push_back(v.base);
                }
            }
        }
    }
    uint64_t totalsize = 0;
    for (const bh_base *base: non_temps) {
        totalsize += bh_base_size(base);
    }
    return totalsize;
}

void pprint(const DAG &dag, const string &filename) {

    //We define a graph and a kernel writer for graphviz
    struct graph_writer {
        const DAG &graph;
        graph_writer(const DAG &g) : graph(g) {};
        void operator()(std::ostream& out) const {
            uint64_t totalcost = 0;

            BOOST_FOREACH(Vertex v, boost::vertices(graph)) {
                totalcost += block_cost(*graph[v]);
            }

            out << "labelloc=\"t\";" << endl;
            out << "label=\"Total cost: " << (double) totalcost;
            out << "\";";
            out << "graph [bgcolor=white, fontname=\"Courier New\"]" << endl;
            out << "node [shape=box color=black, fontname=\"Courier New\"]" << endl;
        }
    };
    struct kernel_writer {
        const DAG &graph;
        kernel_writer(const DAG &g) : graph(g) {};
        void operator()(std::ostream& out, const Vertex& v) const {
            out << "[label=\"Kernel " << v;
            out << ", Cost: " << (double) block_cost(*graph[v]);
            out << ", Instructions: \\l";
            for (const bh_instruction *instr: graph[v]->getAllInstr()) {
                out << *instr << "\\l";
            }
            out << "\"]";
        }
    };
    struct edge_writer {
        const DAG &graph;
        edge_writer(const DAG &g) : graph(g) {};
        void operator()(std::ostream& out, const Edge& e) const {
            Vertex src = source(e, graph);
            Vertex dst = target(e, graph);
            out << "[label=\" ";
            out << (double) weight(*graph[src], *graph[dst]) << " bytes\"";
            out << "]";
        }
    };

    static int count=0;
    stringstream ss;
    ss << filename << "-" << count++ << ".dot";
    ofstream file;
    cout << ss.str() << endl;
    file.open(ss.str());
    boost::write_graphviz(file, dag, kernel_writer(dag), edge_writer(dag), graph_writer(dag));
    file.close();
}

} // graph
} // jitk
} // bohrium
