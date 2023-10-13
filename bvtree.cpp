
#include <iterator>
#include <ranges>
#include <unordered_map>
#include "defs.hpp"

using BVNodesConstItr = std::vector<BVNode*>::const_iterator;

static BBox get_tri_bbox(const std::vector<vec3>& V, const std::array<int, 3>& tri)
{
    BBox bb;
    for (int i = 0; i < 3; ++i)
    {
        bb.cmin = bb.cmin.cwiseMin(V[tri[i]]);
        bb.cmax = bb.cmax.cwiseMax(V[tri[i]]);
    }
    return bb;
}

static BBox get_nodes_bbox(BVNodesConstItr begin, BVNodesConstItr end)
{
    BBox bb;
    for (auto it = begin; it != end; ++it)
    {
        auto& node = *it;
        bb.cmin = bb.cmin.cwiseMin(node->bbox.cmin);
        bb.cmax = bb.cmax.cwiseMax(node->bbox.cmax);
    }
    return bb;
}

static BVNode* tree_create(BVNodesConstItr tris_beg, BVNodesConstItr tris_end);

static BVNode* get_subtree(BVNodesConstItr tris_beg, BVNodesConstItr tris_end)
{
    switch (std::distance(tris_beg, tris_end))
    {
    case 0: return nullptr;
    case 1: return *tris_beg;
    default: return tree_create(tris_beg, tris_end);
    }
}

static BVNode* tree_create(BVNodesConstItr tris_beg, BVNodesConstItr tris_end)
{
    BVNode* root = new BVNode();

    root->tri = -1; // bbox node
    root->bbox = get_nodes_bbox(tris_beg, tris_end);   
    root->nleaves = int(std::distance(tris_beg, tris_end));
    root->ndesc = 0;
    
    int maxDim = (root->bbox.cmax - root->bbox.cmin).maxDim();
    // sort by longest dimension
    std::vector<BVNode*> sortedtris(tris_beg, tris_end);
    ranges::sort(sortedtris, [=](BVNode* lhs, BVNode* rhs) {
        return lhs->bbox.center()[maxDim] < rhs->bbox.center()[maxDim]; });

    // divide equally into 2 subtrees
    auto lhs_size = sortedtris.size() / 2;
    root->left = get_subtree(sortedtris.begin(), sortedtris.begin() + lhs_size);
    root->right = get_subtree(sortedtris.begin() + lhs_size, sortedtris.end());

    if (root->left) root->ndesc += root->left->ndesc + 1;
    if (root->right) root->ndesc += root->right->ndesc + 1;

    return root;
}

static void tree_delete(BVNode* root)
{
    if (!root) return;

    tree_delete(root->left);
    tree_delete(root->right);

    delete root;
}

static constexpr uint nserial_tri = BBox::nserial + 4;
static constexpr uint nserial_bbox = BBox::nserial + 12;

static uint tree_nserial(BVNode* root)
{
    if (!root) return 0;
    return
        root->nleaves * nserial_tri + // tri descendants
        (root->ndesc - root->nleaves) * nserial_bbox + // bbox descendants
        (root->tri == -1 ? nserial_bbox : nserial_tri); // this node
}

static byte* tree_serialize(BVNode* root, byte* const buf, byte* p)
{
    if (!root) return p;

    // bbox, tri
    root->bbox.serialize(p);
    p += BBox::nserial;
    uint d = bswap(uint(root->tri));
    std::memcpy(p, &d, 4);
    p += 4;

    // left, right
    if (root->tri == -1)
    {
        // offsets into buf where left/right nodes are found
        uint lpos = uint(-1), rpos = uint(-1);
        if (root->left)
        {
            lpos = bswap(uint(p + 8 - buf));
            if (root->right)
                rpos = bswap(uint(p + 8 + tree_nserial(root->left) - buf));
        }
        else if (root->right)
            rpos = bswap(uint(p + 8 - buf));

        std::memcpy(p, &lpos, 4); p += 4;
        std::memcpy(p, &rpos, 4); p += 4;
    }

    p = tree_serialize(root->left, buf, p);
    p = tree_serialize(root->right, buf, p);
    return p;
}

BVTree::BVTree(const ModelData& m)
{
    // triangle nodes.
    // these are the leaves
    std::vector<BVNode*> tris;

    for (int i = 0; i < m.F.size(); ++i)
    {
        auto* n = new BVNode();
        n->bbox = get_tri_bbox(m.V, m.F[i]);
        n->left = nullptr;
        n->right = nullptr;
        n->tri = i;
        n->ndesc = 0;
        n->nleaves = 0;
        tris.push_back(n);
    }

    m_root = tree_create(tris.begin(), tris.end());
}

BVTree::~BVTree() { tree_delete(m_root); }

uint BVTree::nserial() const { return tree_nserial(m_root); }

void BVTree::serialize(byte buf[]) const
{
    auto* res = tree_serialize(m_root, buf, buf);
    // check if serialization actually worked
    assert(uint(res - buf) == tree_nserial(m_root)); 
    (void)res;
}
