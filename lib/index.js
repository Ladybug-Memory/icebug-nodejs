'use strict';

/**
 * icebug – Node.js bindings for NetworKit graph analytics
 *
 * This module re-exports the native N-API addon and adds lightweight
 * JavaScript conveniences on top.
 */

const path = require('path');

// Load the compiled native addon.
let native;
try {
  native = require(path.join(__dirname, '..', 'build', 'Release', 'icebug.node'));
} catch (releaseErr) {
  try {
    native = require(path.join(__dirname, '..', 'build', 'Debug', 'icebug.node'));
  } catch (debugErr) {
    throw new Error(
      `Failed to load icebug native addon.\n` +
      `  Release error: ${releaseErr.message}\n` +
      `  Debug error:   ${debugErr.message}\n` +
      `Run 'npm install' or 'npm run build' first.`
    );
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

  // I/O functions
  readMETIS,
  readEdgeList,

  // JS helpers
  graphFromEdges,
  weightedGraphFromEdges,
};
