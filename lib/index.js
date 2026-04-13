'use strict';

/**
 * icebug – Node.js bindings for NetworKit graph analytics
 *
 * This module re-exports the native N-API addon and adds lightweight
 * JavaScript conveniences on top.
 */

const path = require('path');

// ── Load the native addon ───────────────────────────────────────────────────
// Priority:
//   1. prebuilds/{platform}-{arch}/icebug.node  — distributed package
//   2. build/Release/icebug.node                — local `npm run build`
//   3. build/Debug/icebug.node                  — local `npm run build:debug`

let native;
const platformKey = `${process.platform}-${process.arch}`;

try {
  native = require(path.join(__dirname, '..', 'prebuilds', platformKey, 'icebug.node'));
} catch (prebuiltErr) {
  try {
    native = require(path.join(__dirname, '..', 'build', 'Release', 'icebug.node'));
  } catch (releaseErr) {
    try {
      native = require(path.join(__dirname, '..', 'build', 'Debug', 'icebug.node'));
    } catch (debugErr) {
      throw new Error(
        `Failed to load icebug native addon for ${platformKey}.\n` +
        `  Prebuilt error: ${prebuiltErr.message}\n` +
        `  Release error:  ${releaseErr.message}\n` +
        `  Debug error:    ${debugErr.message}\n` +
        `Run 'npm run build' to compile from source, or check that your platform is supported.`
      );
    }
  }
}

// ── Re-export all native classes and functions ──────────────────────────────

const {
  Graph,
  GraphR,
  Betweenness,
  PageRank,
  DegreeCentrality,
  ConnectedComponents,
  Louvain,
  Leiden,
  readMETIS,
  readEdgeList,
} = native;

// ── Higher-level helpers ────────────────────────────────────────────────────

/**
 * Build a Graph from a plain edge list.
 *
 * @param {number}            n        - Number of nodes.
 * @param {[number,number][]} edges    - Array of [u, v] pairs.
 * @param {object}            [opts]
 * @param {boolean}           [opts.weighted=false]
 * @param {boolean}           [opts.directed=false]
 * @returns {Graph}
 */
function graphFromEdges(n, edges, { weighted = false, directed = false } = {}) {
  const g = new Graph(n, weighted, directed);
  for (const [u, v] of edges) g.addEdge(u, v);
  return g;
}

/**
 * Build a Graph from a plain edge list with weights.
 *
 * @param {number}                      n
 * @param {[number,number,number][]}    edges  - Array of [u, v, weight] triples.
 * @param {object}                      [opts]
 * @param {boolean}                     [opts.directed=false]
 * @returns {Graph}
 */
function weightedGraphFromEdges(n, edges, { directed = false } = {}) {
  const g = new Graph(n, true, directed);
  for (const [u, v, w] of edges) g.addEdge(u, v, w);
  return g;
}

module.exports = {
  // Native classes
  // GraphR accepts: (n, directed, outIndices: BigUint64Array, outIndptr: BigUint64Array
  //                 [, inIndices: BigUint64Array, inIndptr: BigUint64Array])
  // The ArrayBuffers are pinned (zero-copy) until the GraphR instance is GC'd.
  Graph,
  GraphR,

  // Algorithm classes
  Betweenness,
  PageRank,
  DegreeCentrality,
  ConnectedComponents,

  // Clustering / community detection
  Louvain,
  Leiden,

  // I/O functions
  readMETIS,
  readEdgeList,

  // JS helpers
  graphFromEdges,
  weightedGraphFromEdges,
};
