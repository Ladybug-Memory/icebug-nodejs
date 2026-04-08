// TypeScript declarations for the icebug Node.js bindings.

/** A node identifier (non-negative integer, safe range 0 – 2^53−1). */
export type NodeId = number;

/** An edge weight. */
export type EdgeWeight = number;

/** Pair representing an unweighted edge. */
export type Edge = [NodeId, NodeId];

/** Triple representing a weighted edge. */
export type WeightedEdge = [NodeId, NodeId, EdgeWeight];

/** Entry in a centrality ranking. */
export interface RankingEntry {
  node:  NodeId;
  score: number;
}

// ── Graph (mutable, vector-backed) ───────────────────────────────────────────

export class Graph {
  /**
   * Create a mutable graph.
   * @param n        - Initial number of nodes (default 0).
   * @param weighted - Enable edge weights (default false).
   * @param directed - Create a directed graph (default false).
   */
  constructor(n?: number, weighted?: boolean, directed?: boolean);

  // Metadata
  numberOfNodes(): number;
  numberOfEdges(): number;
  upperNodeIdBound(): number;
  isWeighted(): boolean;
  isDirected(): boolean;
  isEmpty(): boolean;

  // Mutation
  addNode(): NodeId;
  addEdge(u: NodeId, v: NodeId, weight?: EdgeWeight): boolean;
  removeNode(u: NodeId): void;
  removeEdge(u: NodeId, v: NodeId): void;
  setWeight(u: NodeId, v: NodeId, w: EdgeWeight): void;

  // Queries
  hasNode(u: NodeId): boolean;
  hasEdge(u: NodeId, v: NodeId): boolean;
  degree(u: NodeId): number;
  degreeIn(u: NodeId): number;
  weight(u: NodeId, v: NodeId): EdgeWeight;

  // Iteration
  nodes(): NodeId[];
  neighbors(u: NodeId): NodeId[];
  edges(): Edge[];
  weightedEdges(): WeightedEdge[];
}

// ── GraphR (read-only, Arrow CSR-backed) ─────────────────────────────────────

export class GraphR {
  /**
   * Create a read-only CSR graph.
   *
   * @param n          - Number of nodes.
   * @param directed   - Directed graph?
   * @param outIndices - CSR neighbour indices for outgoing edges.
   * @param outIndptr  - CSR offset array for outgoing edges (length n+1).
   * @param inIndices  - CSR neighbour indices for incoming edges (directed only).
   * @param inIndptr   - CSR offset array for incoming edges (directed only, length n+1).
   */
  constructor(
    n: number,
    directed: boolean,
    outIndices: number[] | Uint32Array | BigUint64Array,
    outIndptr:  number[] | Uint32Array | BigUint64Array,
    inIndices?: number[] | Uint32Array | BigUint64Array,
    inIndptr?:  number[] | Uint32Array | BigUint64Array,
  );

  // Metadata
  numberOfNodes(): number;
  numberOfEdges(): number;
  upperNodeIdBound(): number;
  isWeighted(): boolean;
  isDirected(): boolean;
  isEmpty(): boolean;

  // Queries
  hasNode(u: NodeId): boolean;
  hasEdge(u: NodeId, v: NodeId): boolean;
  degree(u: NodeId): number;
  degreeIn(u: NodeId): number;
  weight(u: NodeId, v: NodeId): EdgeWeight;

  // Iteration
  nodes(): NodeId[];
  neighbors(u: NodeId): NodeId[];
  edges(): Edge[];
}

// ── Betweenness centrality ───────────────────────────────────────────────────

export class Betweenness {
  /**
   * @param graph                  - The input graph.
   * @param normalized             - Normalize scores to [0, 1] (default false).
   * @param computeEdgeCentrality  - Also compute edge betweenness (default false).
   */
  constructor(graph: Graph | GraphR, normalized?: boolean, computeEdgeCentrality?: boolean);

  run(): void;
  hasFinished(): boolean;
  scores(): Float64Array;
  score(v: NodeId): number;
  ranking(): RankingEntry[];
  maximum(): number;
}

// ── PageRank ─────────────────────────────────────────────────────────────────

export class PageRank {
  /**
   * @param graph      - The input graph.
   * @param damp       - Damping factor (default 0.85).
   * @param tol        - Convergence tolerance (default 1e-8).
   * @param normalized - Normalize scores (default false).
   */
  constructor(graph: Graph | GraphR, damp?: number, tol?: number, normalized?: boolean);

  run(): void;
  hasFinished(): boolean;
  scores(): Float64Array;
  score(v: NodeId): number;
  ranking(): RankingEntry[];
  numberOfIterations(): number;
}

// ── DegreeCentrality ─────────────────────────────────────────────────────────

export class DegreeCentrality {
  /**
   * @param graph           - The input graph.
   * @param normalized      - Normalize scores (default false).
   * @param outDeg          - Use out-degree (default true); false = in-degree.
   * @param ignoreSelfLoops - Ignore self-loops (default true).
   */
  constructor(
    graph: Graph | GraphR,
    normalized?: boolean,
    outDeg?: boolean,
    ignoreSelfLoops?: boolean,
  );

  run(): void;
  hasFinished(): boolean;
  scores(): Float64Array;
  score(v: NodeId): number;
  ranking(): RankingEntry[];
}

// ── ConnectedComponents ───────────────────────────────────────────────────────

export class ConnectedComponents {
  /** @param graph - An undirected graph. */
  constructor(graph: Graph | GraphR);

  run(): void;
  hasFinished(): boolean;
  numberOfComponents(): number;
  componentOfNode(u: NodeId): number;
  getComponents(): NodeId[][];
  getComponentSizes(): Record<string, number>;
}

// ── I/O functions ────────────────────────────────────────────────────────────

/**
 * Read a graph in METIS format.
 * @param path - Path to the .graph file.
 * @returns A mutable Graph.
 */
export function readMETIS(path: string): Graph;

/**
 * Read a graph from an edge list file.
 * @param path          - Path to the edge list file.
 * @param separator     - Column separator character (default ' ').
 * @param firstNode     - Index of the first node in the file (default 0).
 * @param directed      - Treat as directed graph (default false).
 * @param commentPrefix - Prefix for comment lines (default '#').
 * @returns A mutable Graph.
 */
export function readEdgeList(
  path:          string,
  separator?:    string,
  firstNode?:    number,
  directed?:     boolean,
  commentPrefix?: string,
): Graph;

// ── JS helpers ────────────────────────────────────────────────────────────────

/**
 * Convenience: build an unweighted Graph from n and an edge list.
 */
export function graphFromEdges(
  n: number,
  edges: Edge[],
  opts?: { weighted?: boolean; directed?: boolean },
): Graph;

/**
 * Convenience: build a weighted Graph from n and a weighted edge list.
 */
export function weightedGraphFromEdges(
  n: number,
  edges: WeightedEdge[],
  opts?: { directed?: boolean },
): Graph;
