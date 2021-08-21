#ifndef OCTREE_OCTREE_H
#define OCTREE_OCTREE_H

#include <stdint.h>
#include <queue>
#include <linmath.h>
#include <fmt/core.h>

namespace Octree {

//         Points and regions
//
//       2-------------------3
//      /|                  /|
//     / |                 / |
//    /  |                /  |
//   6-------------------7   |
//   |   |               |   |
//   |   |               |   |
//   |   |               |   |
//   |   |               |   |
//   |   0---------------|---1
//   |  /                |  /
//   | /                 | /
//   |/                  |/
//   4-------------------5

const int BLOCK_SIZE = 1024;

typedef uint16_t NodeIndex;

struct OctreeNode {
    NodeIndex value;
    NodeIndex parent;
    NodeIndex children[8];
};

struct Octree {
    vec3 center;
    float size;
    OctreeNode *nodes;
    NodeIndex used;
    std::queue<NodeIndex> empty_nodes;
    NodeIndex capacity;
};

void Init(Octree *octree)
{
    octree->nodes = new OctreeNode[BLOCK_SIZE];
    octree->capacity = BLOCK_SIZE;
    octree->nodes[0] = {};
    memcpy(octree->center, vec3{0, 0, 0}, sizeof(vec3));
    octree->size = 4;
}

OctreeNode * GetNode(Octree *octree, NodeIndex node)
{
    return &octree->nodes[node];
}

void ClearNode(Octree *octree, NodeIndex node)
{
    *GetNode(octree, node) = {};
}

void SplitNode(Octree *octree, NodeIndex node)
{
    for (size_t i = 0; i < 8; ++i) {
        NodeIndex index;

        if (octree->used + 1 >= octree->capacity) {
            //TODO: add more space
        }

        if (octree->empty_nodes.size()) {
            index = octree->empty_nodes.front();
            octree->empty_nodes.pop();
        } else {
            index = ++octree->used;
        }

        GetNode(octree, node)->children[i] = index;
        ClearNode(octree, index);
        GetNode(octree, index)->parent = node;
    }   
}

bool HasChildren(Octree *octree, NodeIndex node)
{
    return octree->nodes[node].children[0] 
        || octree->nodes[node].children[1] 
        || octree->nodes[node].children[2] 
        || octree->nodes[node].children[3]
        || octree->nodes[node].children[4] 
        || octree->nodes[node].children[5] 
        || octree->nodes[node].children[6]
        || octree->nodes[node].children[7];
}

const vec3 DEBUG_DIVISION[] = {
    {0.5, 0, 0}, {-0.5, 0, 0}, 
    {0, 0.5, 0}, {0, -0.5, 0},
    {0, 0, 0.5}, {0, 0, -0.5}, 

    {0.5, 0.5, 0}, {-0.5, 0.5, 0}, 
    {0, 0.5, 0.5}, {0, 0.5, -0.5}, 
    {0.5, -0.5, 0}, {-0.5, -0.5, 0}, 
    {0, -0.5, 0.5}, {0, -0.5, -0.5}, 

    {0.5, 0.5, 0}, {0.5, -0.5, 0}, 
    {0.5, 0, 0.5}, {0.5, 0, -0.5}, 
    {-0.5, 0.5, 0}, {-0.5, -0.5, 0}, 
    {-0.5, 0, 0.5}, {-0.5, 0, -0.5}, 

    {0.5, 0, 0.5}, {-0.5, 0, 0.5},
    {0, 0.5, 0.5}, {0, -0.5, 0.5},
    {0.5, 0, -0.5}, {-0.5, 0, -0.5},
    {0, 0.5, -0.5}, {0, -0.5, -0.5},
};

const vec3 CHILDREN_CENTER_OFFSET[] = {
    {-0.5, 0.5, -0.5},
    {0.5, 0.5, -0.5},
    {-0.5, -0.5, -0.5},
    {0.5, -0.5, -0.5},
    {-0.5, 0.5, 0.5},
    {0.5, 0.5, 0.5},
    {-0.5, -0.5, 0.5},
    {0.5, -0.5, 0.5}
};

void InsertPoint(Octree *octree, NodeIndex node, vec3 center, vec3 p, uint16_t depth, uint16_t max_depth)
{
    if (depth >= max_depth) {
        octree->nodes[node].value = 0xffff;
        return;
    }

    if (!HasChildren(octree, node)) {
        SplitNode(octree, node);
    }

    vec3 pn;
    vec3_sub(pn, p, center);

    for (size_t i = 0; i < 8; ++i) {
        if (pn[0] * CHILDREN_CENTER_OFFSET[i][0] * 2 >= 0 && 
            pn[1] * CHILDREN_CENTER_OFFSET[i][1] * 2 >= 0 && 
            pn[2] * CHILDREN_CENTER_OFFSET[i][2] * 2 >= 0) 
        {
            vec3 child_center;
            float scale = 2<<(depth);
            if (!scale) scale = 1.0f;
            vec3_scale(child_center, CHILDREN_CENTER_OFFSET[i], 1.0f * octree->size / scale);
            vec3_add(child_center, center, child_center);
            InsertPoint(octree, octree->nodes[node].children[i], child_center, p, depth + 1, max_depth);

            return;
        }
    }

    fmt::print("what\n");
}

void DebugLineList(Octree *octree, NodeIndex node, vec3 start, vec3 *vert, size_t len, size_t depth, size_t max_depth, size_t *index) 
{
    if (!HasChildren(octree, node) || depth == max_depth) {
        return;
    }

    for (size_t i = 0; i < sizeof(DEBUG_DIVISION) / sizeof(vec3); ++i) {
        vec3 offset;
        
        float scale = 2<<(depth - 1);
        if (!scale) scale = 1.0f;

        vec3_scale(offset, DEBUG_DIVISION[i], 1 * octree->size / scale);
        vec3_add(offset, start, offset);
        
        memcpy(vert[(*index)++], offset, sizeof(vec3));
    }

    for (size_t i = 0; i < 8; ++i) {
        vec3 child_start;
        vec3_scale(child_start, CHILDREN_CENTER_OFFSET[i], 1.0f * octree->size / (2<<depth));
        vec3_add(child_start, start, child_start);

        DebugLineList(octree, octree->nodes[node].children[i], child_start, vert, len, depth + 1, max_depth, index);
    }

    return;
}

static void volume_proc(Octree *octree, NodeIndex q1);
static void face_proc(Octree *octree, NodeIndex q1, NodeIndex q2);
static void edge_proc(Octree *octree, NodeIndex q1, NodeIndex q2, NodeIndex q3, NodeIndex q4);

static void volume_proc(Octree *octree, NodeIndex q1)
{
    uint16_t *child = GetNode(octree, q1)->children;

    for (size_t i = 0; i < 8; ++i) {
        face_proc(octree, child[i]);
    }

    edge_proc(octree, child[0], child[1], child[2], child[3]);
    edge_proc(octree, child[4], child[5], child[6], child[7]);
    edge_proc(octree, child[1], child[5], child[7], child[3]);
    edge_proc(octree, child[0], child[4], child[6], child[2]);
    edge_proc(octree, child[2], child[3], child[7], child[6]);
    edge_proc(octree, child[0], child[4], child[5], child[1]);

    vert_proc(octree, 
        child[0], child[1], 
        child[2], child[3], 
        child[4], child[5], 
        child[6], child[7]
    );
}

static void edge_proc(Octree *octree, NodeIndex q1, NodeIndex q2, NodeIndex q3, NodeIndex q4)
{
    
}

void Cleanup(Octree *octree)
{
    delete[] octree->nodes;
}

}

#endif