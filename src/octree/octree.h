#ifndef OCTREE_OCTREE_H
#define OCTREE_OCTREE_H

#include <stdint.h>
#include <queue>
#include <linmath.h>

namespace Octree {

const int BLOCK_SIZE = 1024;

typedef uint16_t NodeIndex;

struct OctreeNode {
    NodeIndex value;
    NodeIndex parent;
    NodeIndex children[8];
};

struct Octree {
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

const float CHILDREN_CENTER_OFFSET[] = {
    {}
};

size_t DebugLineList(Octree *octree, NodeIndex node, vec3 *vert, size_t len, size_t depth) {
    static size_t index = 0;

    if (!HasChildren(octree, node) || !depth) {
        return index;
    }

    for (size_t i = 0; i < sizeof(DEBUG_DIVISION) / sizeof(vec3); ++i) {
        memcpy(vert[index++], DEBUG_DIVISION[i], sizeof(vec3));
    }

    for (size_t i = 0; i < 8; ++i) {
        DebugLineList(octree, octree->nodes[node].children[i], vert, len, depth - 1);
    }

    return index;
}

void Cleanup(Octree *octree)
{
    delete[] octree->nodes;
}

}

#endif