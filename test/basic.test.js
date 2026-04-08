'use strict';

/**
 * Basic smoke tests for the icebug Node.js bindings.
 * Run with:  node test/basic.test.js
 */

const assert = require('assert');
const path   = require('path');

const {
  Graph,
  GraphR,
  Betweenness,
  PageRank,
  DegreeCentrality,
  ConnectedComponents,
  readMETIS,
  readEdgeList,
  graphFromEdges,
  weightedGraphFromEdges,
} = require('../lib/index.js');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`  ✓ ${name}`);
    passed++;
  } catch (err) {
    console.error(`  ✗ ${name}`);
    console.error(`    ${err.message}`);
    failed++;
  }
}

// ── Graph (GraphW) ───────────────────────────────────────────────────────────
console.log('\nGraph (mutable)');

test('construct empty graph', () => {
  const g = new Graph();
  assert.strictEqual(g.numberOfNodes(), 0);
  assert.strictEqual(g.numberOfEdges(), 0);
});

test('construct graph with n nodes', () => {
  const g = new Graph(5);
  assert.strictEqual(g.numberOfNodes(), 5);
});

test('addNode returns sequential ids', () => {
  const g = new Graph();
  assert.strictEqual(g.addNode(), 0);
  assert.strictEqual(g.addNode(), 1);
  assert.strictEqual(g.addNode(), 2);
});

test('addEdge / hasEdge', () => {
  const g = new Graph(4);
  g.addEdge(0, 1);
  g.addEdge(1, 2);
  assert.strictEqual(g.numberOfEdges(), 2);
  assert.ok(g.hasEdge(0, 1));
  assert.ok(g.hasEdge(1, 0));   // undirected
  assert.ok(!g.hasEdge(0, 2));
});

test('degree', () => {
  const g = new Graph(4);
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  g.addEdge(0, 3);
  assert.strictEqual(g.degree(0), 3);
  assert.strictEqual(g.degree(1), 1);
});

test('neighbors', () => {
  const g = new Graph(4);
  g.addEdge(0, 1);
  g.addEdge(0, 2);
  const nb = g.neighbors(0).sort((a, b) => a - b);
  assert.deepStrictEqual(nb, [1, 2]);
});

test('nodes()', () => {
  const g = new Graph(3);
  const ns = g.nodes().sort((a, b) => a - b);
  assert.deepStrictEqual(ns, [0, 1, 2]);
});

test('edges()', () => {
  const g = new Graph(3);
  g.addEdge(0, 1);
  g.addEdge(1, 2);
  const es = g.edges();
  assert.strictEqual(es.length, 2);
});

test('removeEdge', () => {
  const g = new Graph(3);
  g.addEdge(0, 1);
  g.addEdge(1, 2);
  g.removeEdge(0, 1);
  assert.strictEqual(g.numberOfEdges(), 1);
  assert.ok(!g.hasEdge(0, 1));
});

test('removeNode', () => {
  const g = new Graph(3);
  g.addEdge(0, 1);
  g.removeNode(0);
  assert.ok(!g.hasNode(0));
  assert.ok(g.hasNode(1));
});

test('weighted graph', () => {
  const g = new Graph(3, true, false);
  assert.ok(g.isWeighted());
  g.addEdge(0, 1, 2.5);
  assert.strictEqual(g.weight(0, 1), 2.5);
  g.setWeight(0, 1, 7.0);
  assert.strictEqual(g.weight(0, 1), 7.0);
});

test('directed graph', () => {
  const g = new Graph(3, false, true);
  assert.ok(g.isDirected());
  g.addEdge(0, 1);
  assert.ok(g.hasEdge(0, 1));
  assert.ok(!g.hasEdge(1, 0));
  assert.strictEqual(g.degree(0), 1);
  assert.strictEqual(g.degreeIn(1), 1);
});

// ── GraphR (read-only CSR) ───────────────────────────────────────────────────
console.log('\nGraphR (read-only CSR)');

test('construct from BigUint64Array CSR (undirected triangle)', () => {
  // Triangle: 0-1, 1-2, 0-2
  // Each undirected edge stored in both directions in outIndices:
  //   node 0 → [1, 2], node 1 → [0, 2], node 2 → [0, 1]
  const gr = new GraphR(3, false,
    new BigUint64Array([1n, 2n, 0n, 2n, 0n, 1n]),  // outIndices
    new BigUint64Array([0n, 2n, 4n, 6n])            // outIndptr
  );
  assert.strictEqual(gr.numberOfNodes(), 3);
  assert.strictEqual(gr.numberOfEdges(), 3);
  assert.ok(gr.hasEdge(0, 1));
  assert.ok(gr.hasEdge(1, 2));
  assert.strictEqual(gr.degree(0), 2);
});

test('GraphR neighbors', () => {
  const gr = new GraphR(3, false,
    new BigUint64Array([1n, 2n, 0n, 2n, 0n, 1n]),
    new BigUint64Array([0n, 2n, 4n, 6n])
  );
  const nb = gr.neighbors(0).sort((a, b) => a - b);
  assert.deepStrictEqual(nb, [1, 2]);
});

test('GraphR directed with in-arrays', () => {
  // Directed path 0→1→2
  const gr = new GraphR(3, true,
    new BigUint64Array([1n, 2n]),      // outIndices: 0→1, 1→2
    new BigUint64Array([0n, 1n, 2n, 2n]),  // outIndptr
    new BigUint64Array([0n, 1n]),      // inIndices: 1←0, 2←1
    new BigUint64Array([0n, 0n, 1n, 2n])   // inIndptr
  );
  assert.strictEqual(gr.numberOfEdges(), 2);
  assert.ok(gr.hasEdge(0, 1));
  assert.ok(!gr.hasEdge(1, 0));
  assert.strictEqual(gr.degree(0), 1);
  assert.strictEqual(gr.degreeIn(1), 1);
});

// ── Betweenness ──────────────────────────────────────────────────────────────
console.log('\nBetweenness centrality');

test('betweenness on path graph', () => {
  // Path: 0-1-2-3
  const g = graphFromEdges(4, [[0,1],[1,2],[2,3]]);
  const bc = new Betweenness(g, false);
  bc.run();
  assert.ok(bc.hasFinished());
  const scores = bc.scores();
  assert.strictEqual(scores.length, 4);
  // Node 1 and 2 have the highest betweenness on a path
  assert.ok(scores[1] > scores[0]);
  assert.ok(scores[2] > scores[3]);
});

test('betweenness ranking is sorted descending', () => {
  const g = graphFromEdges(4, [[0,1],[1,2],[2,3]]);
  const bc = new Betweenness(g);
  bc.run();
  const ranking = bc.ranking();
  assert.strictEqual(ranking.length, 4);
  for (let i = 1; i < ranking.length; i++)
    assert.ok(ranking[i-1].score >= ranking[i].score);
});

test('betweenness on GraphR', () => {
  // Path 0-1-2 (undirected, each edge stored both ways)
  const gr = new GraphR(3, false,
    new BigUint64Array([1n, 0n, 2n, 1n]),       // outIndices: 0→1, 1→0, 1→2, 2→1
    new BigUint64Array([0n, 1n, 3n, 4n])        // outIndptr
  );
  const bc = new Betweenness(gr, false);
  bc.run();
  const scores = bc.scores();
  assert.strictEqual(scores.length, 3);
  assert.ok(scores[1] > scores[0]);
});

// ── PageRank ─────────────────────────────────────────────────────────────────
console.log('\nPageRank');

test('pagerank sums to ~1 on undirected graph', () => {
  const g = graphFromEdges(4, [[0,1],[1,2],[2,3],[3,0]]);
  const pr = new PageRank(g, 0.85, 1e-9);
  pr.run();
  assert.ok(pr.hasFinished());
  const scores = pr.scores();
  assert.strictEqual(scores.length, 4);
  const sum = Array.from(scores).reduce((a, b) => a + b, 0);
  assert.ok(Math.abs(sum - 1.0) < 0.01, `sum=${sum}`);
});

test('pagerank numberOfIterations', () => {
  const g = graphFromEdges(4, [[0,1],[1,2],[2,3]]);
  const pr = new PageRank(g);
  pr.run();
  assert.ok(pr.numberOfIterations() > 0);
});

// ── DegreeCentrality ─────────────────────────────────────────────────────────
console.log('\nDegreeCentrality');

test('degree centrality scores match degree', () => {
  const g = new Graph(4);
  g.addEdge(0, 1); g.addEdge(0, 2); g.addEdge(0, 3);
  const dc = new DegreeCentrality(g);
  dc.run();
  const scores = dc.scores();
  assert.strictEqual(scores.length, 4);
  assert.ok(scores[0] > scores[1]);
});

// ── ConnectedComponents ───────────────────────────────────────────────────────
console.log('\nConnectedComponents');

test('single component', () => {
  const g = graphFromEdges(4, [[0,1],[1,2],[2,3]]);
  const cc = new ConnectedComponents(g);
  cc.run();
  assert.strictEqual(cc.numberOfComponents(), 1);
  assert.strictEqual(cc.componentOfNode(0), cc.componentOfNode(3));
});

test('two components', () => {
  const g = new Graph(4);
  g.addEdge(0, 1);
  g.addEdge(2, 3);
  const cc = new ConnectedComponents(g);
  cc.run();
  assert.strictEqual(cc.numberOfComponents(), 2);
  assert.notStrictEqual(cc.componentOfNode(0), cc.componentOfNode(2));
});

test('getComponents returns all nodes', () => {
  const g = graphFromEdges(4, [[0,1],[1,2],[2,3]]);
  const cc = new ConnectedComponents(g);
  cc.run();
  const comps = cc.getComponents();
  assert.strictEqual(comps.length, 1);
  assert.strictEqual(comps[0].length, 4);
});

test('getComponentSizes', () => {
  const g = new Graph(4);
  g.addEdge(0, 1);
  g.addEdge(2, 3);
  const cc = new ConnectedComponents(g);
  cc.run();
  const sizes = cc.getComponentSizes();
  const vals = Object.values(sizes).sort();
  assert.deepStrictEqual(vals, [2, 2]);
});

// ── Helpers ──────────────────────────────────────────────────────────────────
console.log('\nJS helpers');

test('graphFromEdges', () => {
  const g = graphFromEdges(5, [[0,1],[2,4]]);
  assert.strictEqual(g.numberOfNodes(), 5);
  assert.strictEqual(g.numberOfEdges(), 2);
});

test('weightedGraphFromEdges', () => {
  const g = weightedGraphFromEdges(3, [[0,1,1.5],[1,2,2.5]]);
  assert.ok(g.isWeighted());
  assert.ok(Math.abs(g.weight(0,1) - 1.5) < 1e-9);
});

// ── I/O ──────────────────────────────────────────────────────────────────────
console.log('\nI/O');

const inputDir = path.join(__dirname, '..', 'input');

test('readMETIS (jazz.graph)', () => {
  const g = readMETIS(path.join(inputDir, 'jazz.graph'));
  assert.ok(g.numberOfNodes() > 0);
  assert.ok(g.numberOfEdges() > 0);
  console.log(`    jazz.graph: ${g.numberOfNodes()} nodes, ${g.numberOfEdges()} edges`);
});

test('readEdgeList', () => {
  // spaceseparated.edgelist has integer node ids, space-separated, 1-indexed
  const g = readEdgeList(path.join(inputDir, 'spaceseparated.edgelist'), ' ', 1, false, '#');
  assert.ok(g.numberOfNodes() > 0);
  console.log(`    spaceseparated.edgelist: ${g.numberOfNodes()} nodes, ${g.numberOfEdges()} edges`);
});

// ── Summary ──────────────────────────────────────────────────────────────────
console.log(`\n${'─'.repeat(50)}`);
console.log(`Results: ${passed} passed, ${failed} failed`);
if (failed > 0) process.exit(1);
