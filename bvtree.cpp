
#include <iterator>
#include <ranges>
#include "defs.hpp"

using BVNodesConstItr = std::vector<BVNode*>::const_iterator;

static inline BBox get_tri_bbox(const std::vector<vec3>& V, const std::array<int, 3>& tri)
{
    BBox bb;
    for (int i = 0; i < 3; ++i)
    {
        bb.cmin = bb.cmin.cwiseMin(V[tri[i]]);
        bb.cmax = bb.cmax.cwiseMax(V[tri[i]]);
    }
    return bb;
}

static inline BBox get_nodes_bbox(BVNodesConstItr begin, BVNodesConstItr end)
{
    BBox bb;
    for (auto it = begin; it != end; ++it)
    {
        const BVNode* node = *it;
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
    
    const int maxDim = (root->bbox.cmax - root->bbox.cmin).maxDim();
    // sort by longest dimension
    std::vector<BVNode*> sortedtris(tris_beg, tris_end);
    ranges::sort(sortedtris, [=](BVNode* lhs, BVNode* rhs) {
        return lhs->bbox.center()[maxDim] < rhs->bbox.center()[maxDim]; });

    // divide equally into 2 subtrees
    size_t lhs_size = sortedtris.size() / 2;
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

static constexpr uint nserial_leaf = BBox::nserial + 1;
static constexpr uint nserial_bbox = BBox::nserial + 3;

static uint tree_nserial(BVNode* root)
{
    if (!root) return 0;
    return
        root->nleaves * nserial_leaf +
        (root->ndesc - root->nleaves) * nserial_bbox +
        (root->tri == -1 ? nserial_bbox : nserial_leaf);
}

static uint* tree_serialize(BVNode* root, uint* const beg, uint* p)
{
    if (!root) return p;

    // bbox, tri
    root->bbox.serialize(p);
    p += BBox::nserial;
    *p++ = root->tri; 

    if (root->tri == -1)
    {
        // offsets where left/right nodes are found
        uint lpos = uint(-1), rpos = uint(-1);
        if (root->left)
        {
            lpos = uint(p + 2 - beg);
            if (root->right) {
                // right subtree comes after all nodes in left subtree
                rpos = uint(p + 2 + tree_nserial(root->left) - beg);
            }
        }
        else if (root->right) {
            rpos = uint(p + 2 - beg);
        }
        *p++ = lpos; 
        *p++ = rpos;
    }

    p = tree_serialize(root->left, beg, p);
    p = tree_serialize(root->right, beg, p);
    return p;
}

BVTree::BVTree(const SceneData& m) : m_ok(false)
{
    // triangle nodes.
    // these are the leaves
    std::vector<BVNode*> tris;
    tris.resize(m.F.size());

    for (int i = 0; i < int(m.F.size()); ++i)
    {
        auto* n = new BVNode();
        n->bbox = get_tri_bbox(m.V, m.F[i]);
        n->left = nullptr;
        n->right = nullptr;
        n->tri = i;
        n->ndesc = 0;
        n->nleaves = 0;
        tris[i] = n;
    }

    m_root = tree_create(tris.begin(), tris.end());
    m_ok = true;
}

BVTree::~BVTree() { tree_delete(m_root); }

uint BVTree::nserial() const { return tree_nserial(m_root); }

void BVTree::serialize(uint* buf) const
{
    auto* res = tree_serialize(m_root, buf, buf);
    // sanity check
    assert(uint(res - buf) == nserial()); 
    (void)res;
}
