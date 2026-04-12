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

// ── Clustering / community detection ─────────────────────────────────────────

/**
 * The result of a community detection algorithm.
 *
 * `membership[i]` is the internal community id assigned to node `i`.
 * Community ids are **not** guaranteed to be consecutive starting at 0 —
 * use `count` to know how many distinct communities exist.
 */
export interface PartitionResult {
  /** Float64Array of length `numberOfNodes()`: node i → community id. */
  membership: Float64Array;
  /** Number of distinct (non-empty) communities. */
  count: number;
}

/**
 * Louvain community detection (NetworKit PLM — Parallel Louvain Method).
 *
 * Maximises modularity via a greedy, multi-level approach.
 * Operates on **undirected** graphs; weights are respected when present.
 */
export class Louvain {
  /**
   * @param graph    An undirected Graph or GraphR.
   * @param refine   Add a second refinement pass (default false).
   * @param gamma    Resolution parameter — 1.0 is standard modularity (default 1.0).
   *                 Lower values yield fewer, larger communities; higher values more,
   *                 smaller ones.
   * @param maxIter  Maximum move-phase iterations per level (default 32).
   * @param turbo    Faster mode using O(n) extra memory per thread (default true).
   * @param recurse  Use recursive coarsening (default true).
   */
  constructor(
    graph:    Graph | GraphR,
    refine?:  boolean,
    gamma?:   number,
    maxIter?: number,
    turbo?:   boolean,
    recurse?: boolean,
  );

  /** Run the algorithm. */
  run(): void;

  /** Returns true once `run()` has completed successfully. */
  hasFinished(): boolean;

  /** Number of communities in the detected partition. */
  numberOfCommunities(): number;

  /**
   * Community id of node `u`.
   * The id is an opaque integer meaningful only relative to other nodes'
   * community ids (i.e. same id ⟺ same community).
   */
  communityOfNode(u: NodeId): number;

  /**
   * Full partition as a typed array.
   * `membership[i]` is the community id of node `i`.
   */
  getPartition(): PartitionResult;

  /**
   * Communities as an array of node-id arrays.
   * Each inner array contains the ids of all nodes in one community.
   * Communities are returned in ascending order of their internal id.
   */
  getCommunities(): NodeId[][];

  /**
   * Modularity Q ∈ [−0.5, 1.0] of the detected partition.
   * Higher values indicate a stronger community structure.
   */
  modularity(): number;
}

/**
 * Leiden community detection (NetworKit ParallelLeiden).
 *
 * A refined version of Louvain that guarantees (in the sequential reference
 * implementation) that all communities are internally connected.
 *
 * **Note:** The NetworKit parallel implementation may produce a small fraction
 * of internally disconnected communities in some cases.
 *
 * Operates on **undirected** graphs; weights are respected when present.
 */
export class Leiden {
  /**
   * @param graph      An undirected Graph or GraphR.
   * @param iterations Number of full Leiden iterations to run (default 3).
   * @param randomize  Randomise node order each iteration (default true).
   * @param gamma      Resolution parameter — 1.0 is standard modularity (default 1.0).
   */
  constructor(
    graph:       Graph | GraphR,
    iterations?: number,
    randomize?:  boolean,
    gamma?:      number,
  );

  /** Run the algorithm. */
  run(): void;

  /** Returns true once `run()` has completed successfully. */
  hasFinished(): boolean;

  /** Number of communities in the detected partition. */
  numberOfCommunities(): number;

  /**
   * Community id of node `u`.
   * The id is an opaque integer meaningful only relative to other nodes'
   * community ids (i.e. same id ⟺ same community).
   */
  communityOfNode(u: NodeId): number;

  /**
   * Full partition as a typed array.
   * `membership[i]` is the community id of node `i`.
   */
  getPartition(): PartitionResult;

  /**
   * Communities as an array of node-id arrays.
   * Each inner array contains the ids of all nodes in one community.
   * Communities are returned in ascending order of their internal id.
   */
  getCommunities(): NodeId[][];

  /**
   * Modularity Q ∈ [−0.5, 1.0] of the detected partition.
   * Higher values indicate a stronger community structure.
   */
  modularity(): number;
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
