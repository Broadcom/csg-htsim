#!/usr/bin/env python3
"""
Generate traffic matrices for htsim Fat Tree simulations.

Fat Tree topology requires: N = K³/4 where K is even.
Valid node counts: 2, 16, 54, 128, 250, 432, 686, 1024, 1458, ...
"""

import sys
import random
import argparse


def valid_fat_tree_sizes(max_nodes=2000):
    """Calculate valid Fat Tree node counts."""
    valid = []
    k = 2
    while True:
        nodes = (k ** 3) // 4
        if nodes > max_nodes:
            break
        valid.append((k, nodes))
        k += 2
    return valid


def generate_permutation(nodes, flow_size=10000000, output=None):
    """Generate a permutation traffic matrix where each node sends to one other unique node."""
    # Create a permutation
    dests = list(range(nodes))
    random.shuffle(dests)

    # Ensure no self-connections
    for src in range(nodes):
        if dests[src] == src:
            # Swap with next destination
            swap_with = (src + 1) % nodes
            dests[src], dests[swap_with] = dests[swap_with], dests[src]

    # Write to output
    if output:
        f = open(output, 'w')
    else:
        f = sys.stdout

    print(f"# Permutation traffic matrix", file=f)
    print(f"# {nodes} nodes, each sending to one other unique node", file=f)
    print(f"# Flow size: {flow_size} bytes ({flow_size/1000000:.1f} MB)", file=f)
    print(f"Nodes {nodes}", file=f)
    print(f"Connections {nodes}", file=f)

    for src in range(nodes):
        print(f"{src}->{dests[src]} start 0 size {flow_size}", file=f)

    if output:
        f.close()
        print(f"Generated traffic matrix: {output}", file=sys.stderr)


def generate_incast(nodes, center=0, sources=None, flow_size=10000000, output=None):
    """Generate an incast traffic matrix where multiple nodes send to one destination."""
    if sources is None:
        sources = nodes - 1  # All nodes except center

    # Write to output
    if output:
        f = open(output, 'w')
    else:
        f = sys.stdout

    print(f"# Incast traffic matrix", file=f)
    print(f"# {sources} nodes sending to node {center}", file=f)
    print(f"# Flow size: {flow_size} bytes ({flow_size/1000000:.1f} MB)", file=f)
    print(f"Nodes {nodes}", file=f)
    print(f"Connections {sources}", file=f)

    conn_count = 0
    for src in range(nodes):
        if src != center and conn_count < sources:
            print(f"{src}->{center} start 0 size {flow_size}", file=f)
            conn_count += 1

    if output:
        f.close()
        print(f"Generated traffic matrix: {output}", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(
        description='Generate traffic matrices for htsim Fat Tree simulations',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Valid Fat Tree node counts (N = K³/4):
  K=2:    2 nodes
  K=4:   16 nodes
  K=6:   54 nodes
  K=8:  128 nodes
  K=10: 250 nodes
  K=12: 432 nodes
  K=14: 686 nodes
  K=16: 1024 nodes

Examples:
  # Generate 16-node permutation to stdout
  python3 generate_traffic_matrix.py permutation 16

  # Generate 432-node permutation to file
  python3 generate_traffic_matrix.py permutation 432 -o permutation_432.tm

  # Generate incast with 100 sources to node 0
  python3 generate_traffic_matrix.py incast 128 --sources 100 -o incast.tm

  # List valid node counts
  python3 generate_traffic_matrix.py list
"""
    )

    subparsers = parser.add_subparsers(dest='command', help='Traffic pattern type')

    # List command
    list_parser = subparsers.add_parser('list', help='List valid Fat Tree node counts')
    list_parser.add_argument('--max', type=int, default=2000, help='Maximum node count to show')

    # Permutation command
    perm_parser = subparsers.add_parser('permutation', help='Generate permutation traffic')
    perm_parser.add_argument('nodes', type=int, help='Number of nodes')
    perm_parser.add_argument('-o', '--output', help='Output file (default: stdout)')
    perm_parser.add_argument('-s', '--size', type=int, default=10000000,
                            help='Flow size in bytes (default: 10000000 = 10MB)')
    perm_parser.add_argument('--seed', type=int, help='Random seed for reproducibility')

    # Incast command
    incast_parser = subparsers.add_parser('incast', help='Generate incast traffic')
    incast_parser.add_argument('nodes', type=int, help='Number of nodes')
    incast_parser.add_argument('-o', '--output', help='Output file (default: stdout)')
    incast_parser.add_argument('-s', '--size', type=int, default=10000000,
                              help='Flow size in bytes (default: 10000000 = 10MB)')
    incast_parser.add_argument('--center', type=int, default=0,
                              help='Destination node for incast (default: 0)')
    incast_parser.add_argument('--sources', type=int,
                              help='Number of sources (default: all except center)')
    incast_parser.add_argument('--seed', type=int, help='Random seed for reproducibility')

    args = parser.parse_args()

    if args.command == 'list':
        valid = valid_fat_tree_sizes(args.max)
        print("Valid Fat Tree node counts (N = K³/4):")
        for k, nodes in valid:
            print(f"  K={k:2d}: {nodes:4d} nodes")
        return

    if args.command == 'permutation':
        # Check if valid
        valid_sizes = [n for k, n in valid_fat_tree_sizes()]
        if args.nodes not in valid_sizes:
            print(f"Error: {args.nodes} is not a valid Fat Tree size.", file=sys.stderr)
            print(f"Valid sizes: {', '.join(map(str, valid_sizes[:10]))}, ...", file=sys.stderr)
            print(f"Run 'python3 {sys.argv[0]} list' for full list.", file=sys.stderr)
            sys.exit(1)

        if args.seed is not None:
            random.seed(args.seed)

        generate_permutation(args.nodes, args.size, args.output)

    elif args.command == 'incast':
        # Check if valid
        valid_sizes = [n for k, n in valid_fat_tree_sizes()]
        if args.nodes not in valid_sizes:
            print(f"Error: {args.nodes} is not a valid Fat Tree size.", file=sys.stderr)
            print(f"Valid sizes: {', '.join(map(str, valid_sizes[:10]))}, ...", file=sys.stderr)
            print(f"Run 'python3 {sys.argv[0]} list' for full list.", file=sys.stderr)
            sys.exit(1)

        if args.seed is not None:
            random.seed(args.seed)

        generate_incast(args.nodes, args.center, args.sources, args.size, args.output)

    else:
        parser.print_help()


if __name__ == '__main__':
    main()
