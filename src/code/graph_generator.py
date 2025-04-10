#!/usr/bin/env python3
"""
graph_generator.py

A utility to generate various types of graphs for graph coloring experiments.
This script can create different graph topologies for benchmarking coloring algorithms:
- Random (Erdős-Rényi) graphs
- Grid graphs
- Scale-free (Barabási-Albert) graphs
- Complete graphs
- Bipartite graphs
- Small-world (Watts-Strogatz) graphs

Each graph is saved in a simple edge list format with a header containing the number of vertices and edges.
"""

import os
import sys
import random
import argparse
import numpy as np
from typing import List, Tuple


def write_graph_to_file(edges: List[Tuple[int, int]], num_vertices: int, filename: str):
    """Write a graph to a file in edge list format."""
    # Count actual number of edges
    num_edges = len(edges)
    
    # Create directory if it doesn't exist
    os.makedirs(os.path.dirname(os.path.abspath(filename)), exist_ok=True)
    
    with open(filename, 'w') as f:
        # Write header with number of vertices and edges
        f.write(f"{num_vertices} {num_edges}\n")
        
        # Write each edge
        for u, v in edges:
            f.write(f"{u} {v}\n")
    
    print(f"Generated graph with {num_vertices} vertices and {num_edges} edges")
    print(f"Saved to {filename}")


def generate_random_graph(num_vertices: int, edge_probability: float, filename: str):
    """Generate a random Erdős-Rényi graph."""
    if num_vertices <= 0 or edge_probability < 0 or edge_probability > 1:
        print("Invalid parameters for random graph")
        return
    
    edges = []
    
    # Generate random edges
    for u in range(num_vertices):
        for v in range(u + 1, num_vertices):
            if random.random() < edge_probability:
                edges.append((u, v))
    
    write_graph_to_file(edges, num_vertices, filename)


def generate_grid_graph(width: int, height: int, filename: str):
    """Generate a 2D grid graph."""
    if width <= 0 or height <= 0:
        print("Invalid parameters for grid graph")
        return
    
    num_vertices = width * height
    edges = []
    
    # Generate grid edges
    for y in range(height):
        for x in range(width):
            vertex = y * width + x
            
            # Connect to right neighbor
            if x < width - 1:
                edges.append((vertex, vertex + 1))
            
            # Connect to bottom neighbor
            if y < height - 1:
                edges.append((vertex, vertex + width))
    
    write_graph_to_file(edges, num_vertices, filename)


def generate_scale_free_graph(num_vertices: int, min_edges: int, filename: str):
    """Generate a scale-free Barabási-Albert graph."""
    if num_vertices <= 0 or min_edges <= 0 or min_edges >= num_vertices:
        print("Invalid parameters for scale-free graph")
        return
    
    # Start with a complete graph on min_edges+1 vertices
    nodes = list(range(min_edges + 1))
    edges = [(i, j) for i in range(min_edges + 1) for j in range(i + 1, min_edges + 1)]
    
    # Track the degree of each node
    degree = [min_edges for _ in range(min_edges + 1)]
    total_degree = min_edges * (min_edges + 1)
    
    # Add remaining vertices
    for new_node in range(min_edges + 1, num_vertices):
        # Add min_edges edges from new_node to existing nodes
        added_edges = 0
        while added_edges < min_edges:
            # Choose a target node with probability proportional to its degree
            target = random.choices(nodes, weights=degree, k=1)[0]
            
            # Add the edge if it doesn't already exist
            if (new_node, target) not in edges and (target, new_node) not in edges:
                edges.append((new_node, target))
                degree[target] += 1
                added_edges += 1
        
        # Add the new node to the list and update its degree
        nodes.append(new_node)
        degree.append(min_edges)
        total_degree += 2 * min_edges
    
    write_graph_to_file(edges, num_vertices, filename)


def generate_complete_graph(num_vertices: int, filename: str):
    """Generate a complete graph where every vertex is connected to every other vertex."""
    if num_vertices <= 0:
        print("Invalid parameters for complete graph")
        return
    
    edges = [(i, j) for i in range(num_vertices) for j in range(i + 1, num_vertices)]
    write_graph_to_file(edges, num_vertices, filename)


def generate_bipartite_graph(left_vertices: int, right_vertices: int, edge_probability: float, filename: str):
    """Generate a bipartite graph with two sets of vertices."""
    if left_vertices <= 0 or right_vertices <= 0 or edge_probability < 0 or edge_probability > 1:
        print("Invalid parameters for bipartite graph")
        return
    
    num_vertices = left_vertices + right_vertices
    edges = []
    
    # Generate edges between the two partitions
    for u in range(left_vertices):
        for v in range(left_vertices, num_vertices):
            if random.random() < edge_probability:
                edges.append((u, v))
    
    write_graph_to_file(edges, num_vertices, filename)


def generate_small_world_graph(num_vertices: int, k: int, rewire_prob: float, filename: str):
    """Generate a small-world Watts-Strogatz graph."""
    if num_vertices <= 0 or k <= 0 or k >= num_vertices or rewire_prob < 0 or rewire_prob > 1:
        print("Invalid parameters for small-world graph")
        return
    
    # Ensure k is even
    k = k if k % 2 == 0 else k + 1
    
    # Create a ring lattice
    edges = []
    for i in range(num_vertices):
        for j in range(1, k // 2 + 1):
            edges.append((i, (i + j) % num_vertices))
    
    # Rewire edges
    for i in range(num_vertices):
        for j in range(1, k // 2 + 1):
            if random.random() < rewire_prob:
                # Remove edge
                edge = (i, (i + j) % num_vertices)
                if edge in edges:
                    edges.remove(edge)
                
                # Add new random edge
                while True:
                    new_target = random.randint(0, num_vertices - 1)
                    if new_target != i and (i, new_target) not in edges and (new_target, i) not in edges:
                        edges.append((i, new_target))
                        break
    
    write_graph_to_file(edges, num_vertices, filename)


def main():
    parser = argparse.ArgumentParser(description='Generate graphs for coloring experiments')
    subparsers = parser.add_subparsers(dest='graph_type', help='type of graph to generate')
    
    # Random graph
    random_parser = subparsers.add_parser('random', help='generate random Erdős-Rényi graph')
    random_parser.add_argument('num_vertices', type=int, help='number of vertices')
    random_parser.add_argument('edge_probability', type=float, help='probability of edge creation')
    random_parser.add_argument('output_file', help='output file name')
    
    # Grid graph
    grid_parser = subparsers.add_parser('grid', help='generate 2D grid graph')
    grid_parser.add_argument('width', type=int, help='grid width')
    grid_parser.add_argument('height', type=int, help='grid height')
    grid_parser.add_argument('output_file', help='output file name')
    
    # Scale-free graph
    scalefree_parser = subparsers.add_parser('scale-free', help='generate scale-free Barabási-Albert graph')
    scalefree_parser.add_argument('num_vertices', type=int, help='number of vertices')
    scalefree_parser.add_argument('min_edges', type=int, help='number of edges to attach from a new node to existing nodes')
    scalefree_parser.add_argument('output_file', help='output file name')
    
    # Complete graph
    complete_parser = subparsers.add_parser('complete', help='generate complete graph')
    complete_parser.add_argument('num_vertices', type=int, help='number of vertices')
    complete_parser.add_argument('output_file', help='output file name')
    
    # Bipartite graph
    bipartite_parser = subparsers.add_parser('bipartite', help='generate bipartite graph')
    bipartite_parser.add_argument('left_vertices', type=int, help='number of vertices in left partition')
    bipartite_parser.add_argument('right_vertices', type=int, help='number of vertices in right partition')
    bipartite_parser.add_argument('edge_probability', type=float, help='probability of edge creation')
    bipartite_parser.add_argument('output_file', help='output file name')
    
    # Small-world graph
    smallworld_parser = subparsers.add_parser('small-world', help='generate small-world Watts-Strogatz graph')
    smallworld_parser.add_argument('num_vertices', type=int, help='number of vertices')
    smallworld_parser.add_argument('k', type=int, help='mean degree (average number of nearest neighbors)')
    smallworld_parser.add_argument('rewire_prob', type=float, help='probability of rewiring each edge')
    smallworld_parser.add_argument('output_file', help='output file name')
    
    args = parser.parse_args()
    
    # Set random seed for reproducibility
    random.seed(42)
    np.random.seed(42)
    
    if args.graph_type == 'random':
        generate_random_graph(args.num_vertices, args.edge_probability, args.output_file)
    elif args.graph_type == 'grid':
        generate_grid_graph(args.width, args.height, args.output_file)
    elif args.graph_type == 'scale-free':
        generate_scale_free_graph(args.num_vertices, args.min_edges, args.output_file)
    elif args.graph_type == 'complete':
        generate_complete_graph(args.num_vertices, args.output_file)
    elif args.graph_type == 'bipartite':
        generate_bipartite_graph(args.left_vertices, args.right_vertices, args.edge_probability, args.output_file)
    elif args.graph_type == 'small-world':
        generate_small_world_graph(args.num_vertices, args.k, args.rewire_prob, args.output_file)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()