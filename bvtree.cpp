
#include <iterator>
#include <ranges>
#include "defs.hpp"

using BVNodesConstItr = std::vector<BVNode*>::const_iterator;

static BVNode* make_bvtree(BVNodesConstItr tris_beg, BVNodesConstItr tris_end);


static BBox tri_bbox(const std::vector<vec3>& V, const std::array<int, 3>& tri)
{
    BBox bb;
    for (int i = 0; i < 3; ++i)
    {
        bb.cmin = bb.cmin.cwiseMin(V[tri[i]]);
        bb.cmax = bb.cmax.cwiseMax(V[tri[i]]);
    }
    return bb;
}

static BBox nodes_bbox(BVNodesConstItr begin, BVNodesConstItr end)
{
    BBox bb;
    for (auto it = begin; it != end; ++it)
    {
        auto node = *it;
        bb.cmin = bb.cmin.cwiseMin(node->bbox.cmin);
        bb.cmax = bb.cmax.cwiseMax(node->bbox.cmax);
    }
    return bb;
}

static BVNode* get_subtree(BVNodesConstItr tris_beg, BVNodesConstItr tris_end)
{
    switch (std::distance(tris_beg, tris_end))
    {
    case 0: return nullptr;
    case 1: return *tris_beg;
    default: return make_bvtree(tris_beg, tris_end); // recurse
    }
}

static BVNode* make_bvtree(BVNodesConstItr tris_beg, BVNodesConstItr tris_end)
{
    BVNode* root = new BVNode();
    root->tri = -1; // bbox node
    root->bbox = nodes_bbox(tris_beg, tris_end);
    
    int maxDim = (root->bbox.cmax - root->bbox.cmin).maxDim();
    // sort by longest dimension
    std::vector<BVNode*> sortedtris(tris_beg, tris_end);
    ranges::sort(sortedtris, [=](BVNode* lhs, BVNode* rhs) {
        return lhs->bbox.center()[maxDim] < rhs->bbox.center()[maxDim]; });

    // divide equally into 2 subtrees
    auto lhs_size = sortedtris.size() / 2;
    root->left = get_subtree(sortedtris.begin(), sortedtris.begin() + lhs_size);
    root->right = get_subtree(sortedtris.begin() + lhs_size, sortedtris.end());

    return root;
}

static std::size_t nsbytes(BVNode* root)
{
    if (!root) return 0;
    
    std::size_t nbytes = 0;
    if (root->tri == -1)
    {

    }

    nbytes += nsbytes(root->left);
    nbytes += nsbytes(root->right);
    return nbytes;
}

BVTree::BVTree(const ModelData& m)
{
    // triangle nodes.
    // these are the leaves
    std::vector<BVNode*> tris;

    for (int i = 0; i < m.F.size(); ++i)
    {
        auto* node = new BVNode();
        node->bbox = tri_bbox(m.V, m.F[i]);
        node->left = nullptr;
        node->right = nullptr;
        node->tri = i;        
        tris.push_back(node);
    }

    m_root = make_bvtree(tris.begin(), tris.end());
}

BVTree::~BVTree()
{

}
