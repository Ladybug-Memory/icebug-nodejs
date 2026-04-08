/*
 * addon.cpp – Node.js N-API bindings for icebug / NetworKit
 *
 * Exposed JS classes:
 *   Graph              – wraps NetworKit::GraphW (mutable, vector-based adjacency lists)
 *   GraphR             – wraps NetworKit::GraphR (read-only, Arrow CSR arrays)
 *   Betweenness        – wraps NetworKit::Betweenness
 *   PageRank           – wraps NetworKit::PageRank
 *   DegreeCentrality   – wraps NetworKit::DegreeCentrality
 *   ConnectedComponents– wraps NetworKit::ConnectedComponents
 *
 * Exposed JS functions:
 *   readMETIS(path)                             → Graph
 *   readEdgeList(path, sep, firstNode, directed, commentPrefix) → Graph
 */

#include <napi.h>

#include <networkit/graph/GraphW.hpp>
#include <networkit/graph/GraphR.hpp>
#include <networkit/centrality/Betweenness.hpp>
#include <networkit/centrality/DegreeCentrality.hpp>
#include <networkit/centrality/PageRank.hpp>
#include <networkit/components/ConnectedComponents.hpp>
#include <networkit/io/EdgeListReader.hpp>
#include <networkit/io/METISGraphReader.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Napi;
using namespace NetworKit;

// ============================================================================
// Forward declarations (constructors defined later)
// ============================================================================
class GraphWrapper;
class GraphRWrapper;

Napi::FunctionReference g_GraphCtor;
Napi::FunctionReference g_GraphRCtor;

// ============================================================================
// Helpers
// ============================================================================

/** Wrap a C++ exception into a Napi::Error and throw it. */
static void throwError(Napi::Env env, const std::exception &e) {
    Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
}

/** Convert a JS value to a node (uint64_t).  Accepts Number or BigInt. */
static node jsToNode(const Napi::Value &v) {
    if (v.IsBigInt()) {
        bool lossless = true;
        return v.As<Napi::BigInt>().Uint64Value(&lossless);
    }
    return static_cast<node>(v.As<Napi::Number>().DoubleValue());
}

/** Wrap a node id as a JS Number (safe for ids < 2^53). */
static Napi::Value nodeToJs(Napi::Env env, node u) {
    return Napi::Number::New(env, static_cast<double>(u));
}

// ============================================================================
// GraphWrapper  (JS class: Graph)
//   Wraps GraphW – the mutable, vector-based NetworKit graph.
// ============================================================================
class GraphWrapper : public Napi::ObjectWrap<GraphWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);

    /**
     * JS: new Graph(n=0, weighted=false, directed=false)
     *
     * Internal: when info[0].IsExternal() the wrapper takes ownership of an
     * existing GraphW* pointer (used by readMETIS / readEdgeList factories).
     */
    explicit GraphWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<GraphWrapper>(info) {
        if (info.Length() == 1 && info[0].IsExternal()) {
            // Factory path: take ownership of an externally-created GraphW.
            auto *raw = info[0].As<Napi::External<GraphW>>().Data();
            graph_.reset(raw);
        } else {
            count n = 0;
            bool weighted = false;
            bool directed = false;
            if (info.Length() >= 1) n = static_cast<count>(info[0].As<Napi::Number>().DoubleValue());
            if (info.Length() >= 2) weighted = info[1].As<Napi::Boolean>();
            if (info.Length() >= 3) directed = info[2].As<Napi::Boolean>();
            try {
                graph_ = std::make_unique<GraphW>(n, weighted, directed);
            } catch (const std::exception &e) {
                throwError(info.Env(), e);
            }
        }
    }

    /** Access the underlying GraphW (used by algorithm wrappers). */
    GraphW *graph() const { return graph_.get(); }
    /** Access as base Graph& (used by algorithm wrappers). */
    const Graph &baseGraph() const { return *graph_; }

private:
    std::unique_ptr<GraphW> graph_;

    // ---- node / edge counts ----

    Napi::Value NumberOfNodes(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), static_cast<double>(graph_->numberOfNodes()));
    }
    Napi::Value NumberOfEdges(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), static_cast<double>(graph_->numberOfEdges()));
    }
    Napi::Value UpperNodeIdBound(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), static_cast<double>(graph_->upperNodeIdBound()));
    }

    // ---- mutation ----

    Napi::Value AddNode(const Napi::CallbackInfo &info) {
        try {
            node v = graph_->addNode();
            return nodeToJs(info.Env(), v);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value AddEdge(const Napi::CallbackInfo &info) {
        if (info.Length() < 2) {
            Napi::TypeError::New(info.Env(), "addEdge(u, v [, weight])").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            node u = jsToNode(info[0]);
            node v = jsToNode(info[1]);
            edgeweight w = (info.Length() >= 3) ? info[2].As<Napi::Number>().DoubleValue()
                                                 : defaultEdgeWeight;
            bool ok = graph_->addEdge(u, v, w);
            return Napi::Boolean::New(info.Env(), ok);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value RemoveNode(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "removeNode(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            graph_->removeNode(jsToNode(info[0]));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
        return info.Env().Undefined();
    }
    Napi::Value RemoveEdge(const Napi::CallbackInfo &info) {
        if (info.Length() < 2) {
            Napi::TypeError::New(info.Env(), "removeEdge(u, v)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            graph_->removeEdge(jsToNode(info[0]), jsToNode(info[1]));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
        return info.Env().Undefined();
    }
    Napi::Value SetWeight(const Napi::CallbackInfo &info) {
        if (info.Length() < 3) {
            Napi::TypeError::New(info.Env(), "setWeight(u, v, w)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            graph_->setWeight(jsToNode(info[0]), jsToNode(info[1]),
                              info[2].As<Napi::Number>().DoubleValue());
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
        return info.Env().Undefined();
    }

    // ---- predicates ----

    Napi::Value HasNode(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) return Napi::Boolean::New(info.Env(), false);
        return Napi::Boolean::New(info.Env(), graph_->hasNode(jsToNode(info[0])));
    }
    Napi::Value HasEdge(const Napi::CallbackInfo &info) {
        if (info.Length() < 2) return Napi::Boolean::New(info.Env(), false);
        return Napi::Boolean::New(info.Env(),
                                  graph_->hasEdge(jsToNode(info[0]), jsToNode(info[1])));
    }
    Napi::Value IsWeighted(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), graph_->isWeighted());
    }
    Napi::Value IsDirected(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), graph_->isDirected());
    }
    Napi::Value IsEmpty(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), graph_->isEmpty());
    }

    // ---- per-node properties ----

    Napi::Value Degree(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "degree(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            return Napi::Number::New(info.Env(),
                                     static_cast<double>(graph_->degree(jsToNode(info[0]))));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value DegreeIn(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "degreeIn(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            return Napi::Number::New(info.Env(),
                                     static_cast<double>(graph_->degreeIn(jsToNode(info[0]))));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value Weight(const Napi::CallbackInfo &info) {
        if (info.Length() < 2) {
            Napi::TypeError::New(info.Env(), "weight(u, v)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        return Napi::Number::New(
            info.Env(),
            graph_->weight(jsToNode(info[0]), jsToNode(info[1])));
    }

    // ---- iteration helpers ----

    /** neighbors(u) → number[] */
    Napi::Value Neighbors(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "neighbors(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            node u = jsToNode(info[0]);
            Napi::Array result = Napi::Array::New(info.Env());
            uint32_t idx = 0;
            graph_->forNeighborsOf(u, [&](node v) {
                result[idx++] = nodeToJs(info.Env(), v);
            });
            return result;
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }

    /** nodes() → number[] – all existing node ids */
    Napi::Value Nodes(const Napi::CallbackInfo &info) {
        Napi::Array result = Napi::Array::New(info.Env());
        uint32_t idx = 0;
        graph_->forNodes([&](node u) {
            result[idx++] = nodeToJs(info.Env(), u);
        });
        return result;
    }

    /** edges() → [number, number][] – each entry is [u, v] */
    Napi::Value Edges(const Napi::CallbackInfo &info) {
        Napi::Array result = Napi::Array::New(info.Env());
        uint32_t idx = 0;
        graph_->forEdges([&](node u, node v) {
            Napi::Array pair = Napi::Array::New(info.Env(), 2);
            pair[0u] = nodeToJs(info.Env(), u);
            pair[1u] = nodeToJs(info.Env(), v);
            result[idx++] = pair;
        });
        return result;
    }

    /** weightedEdges() → [number, number, number][] – [u, v, weight] */
    Napi::Value WeightedEdges(const Napi::CallbackInfo &info) {
        Napi::Array result = Napi::Array::New(info.Env());
        uint32_t idx = 0;
        graph_->forEdges([&](node u, node v, edgeweight w) {
            Napi::Array triple = Napi::Array::New(info.Env(), 3);
            triple[0u] = nodeToJs(info.Env(), u);
            triple[1u] = nodeToJs(info.Env(), v);
            triple[2u] = Napi::Number::New(info.Env(), w);
            result[idx++] = triple;
        });
        return result;
    }
};

// ============================================================================
// GraphRWrapper  (JS class: GraphR)
//   Wraps GraphR – read-only CSR-backed graph.
//
// Memory strategy
// ───────────────
// GraphR stores Arrow arrays that hold raw pointers into the underlying
// buffers.  We need those buffers to stay alive for the lifetime of the C++
// graph object.  The canonical JS source for the data is a BigUint64Array
// (8 bytes per element, matching NetworKit's node / index = uint64_t).
//
// Each BigUint64Array argument is pinned by holding a strong Napi::Reference
// to its backing ArrayBuffer.  The reference count is incremented to 1 in
// the constructor and is reset (to 0, allowing GC) in the ObjectWrap
// finaliser, which fires when the JS GraphR object itself is collected.
// The Arrow Buffer wraps the raw pointer with no ownership (no deleter), so
// Arrow will never try to free the memory – that duty stays with V8.
// ============================================================================
class GraphRWrapper : public Napi::ObjectWrap<GraphRWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);

    /**
     * JS: new GraphR(n, directed,
     *                outIndices: BigUint64Array, outIndptr: BigUint64Array
     *               [, inIndices: BigUint64Array, inIndptr: BigUint64Array])
     *
     * outIndices / outIndptr must be BigUint64Array (uint64 elements).
     * For directed graphs pass inIndices and inIndptr as well.
     */
    explicit GraphRWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<GraphRWrapper>(info) {
        if (info.Length() < 4) {
            Napi::TypeError::New(info.Env(),
                "GraphR(n, directed, outIndices: BigUint64Array, outIndptr: BigUint64Array"
                " [, inIndices: BigUint64Array, inIndptr: BigUint64Array])")
                .ThrowAsJavaScriptException();
            return;
        }
        try {
            count n        = static_cast<count>(info[0].As<Napi::Number>().DoubleValue());
            bool  directed = info[1].As<Napi::Boolean>();

            // Pin a BigUint64Array: keep its ArrayBuffer alive and return a
            // zero-copy arrow::UInt64Array wrapping the raw memory.
            auto pinArray = [&](const Napi::Value &val, const char *argName)
                -> std::pair<Napi::Reference<Napi::ArrayBuffer>,
                             std::shared_ptr<arrow::UInt64Array>>
            {
                if (!val.IsTypedArray()) {
                    throw std::runtime_error(
                        std::string(argName) + " must be a BigUint64Array");
                }
                auto ta = val.As<Napi::TypedArray>();
                if (ta.TypedArrayType() != napi_biguint64_array) {
                    throw std::runtime_error(
                        std::string(argName) + " must be a BigUint64Array (got wrong TypedArray type)");
                }

                // Pin the backing ArrayBuffer so V8 won't GC it while graph_ lives.
                Napi::ArrayBuffer ab  = ta.ArrayBuffer();
                auto              ref = Napi::Reference<Napi::ArrayBuffer>::New(ab, 1);

                // Wrap the raw bytes as an Arrow buffer – zero copy, no ownership.
                const uint8_t *raw = static_cast<const uint8_t *>(ab.Data())
                                     + ta.ByteOffset();
                int64_t byteLen    = static_cast<int64_t>(ta.ElementLength() * sizeof(uint64_t));
                auto arrowBuf = std::make_shared<arrow::Buffer>(raw, byteLen);
                auto arrowArr = std::make_shared<arrow::UInt64Array>(
                    static_cast<int64_t>(ta.ElementLength()), arrowBuf);

                return {std::move(ref), std::move(arrowArr)};
            };

            auto [outIdxRef, outIdxArr] = pinArray(info[2], "outIndices");
            auto [outPtrRef, outPtrArr] = pinArray(info[3], "outIndptr");
            outIdxRef_ = std::move(outIdxRef);
            outPtrRef_ = std::move(outPtrRef);

            std::shared_ptr<arrow::UInt64Array> inIdxArr, inPtrArr;
            if (info.Length() >= 6
                    && !info[4].IsUndefined() && !info[4].IsNull()
                    && !info[5].IsUndefined() && !info[5].IsNull()) {
                auto [inIdxRef, inIdxArr_] = pinArray(info[4], "inIndices");
                auto [inPtrRef, inPtrArr_] = pinArray(info[5], "inIndptr");
                inIdxRef_ = std::move(inIdxRef);
                inPtrRef_ = std::move(inPtrRef);
                inIdxArr  = std::move(inIdxArr_);
                inPtrArr  = std::move(inPtrArr_);
            }

            graph_ = std::make_unique<GraphR>(n, directed,
                                              outIdxArr, outPtrArr,
                                              inIdxArr,  inPtrArr);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
    }

    const Graph &baseGraph() const { return *graph_; }

private:
    std::unique_ptr<GraphR> graph_;

    // Pinned JS ArrayBuffers – prevent V8 from collecting the backing memory
    // while graph_ (and its Arrow arrays) are still alive.
    Napi::Reference<Napi::ArrayBuffer> outIdxRef_;
    Napi::Reference<Napi::ArrayBuffer> outPtrRef_;
    Napi::Reference<Napi::ArrayBuffer> inIdxRef_;
    Napi::Reference<Napi::ArrayBuffer> inPtrRef_;

    // ---- counts ----
    Napi::Value NumberOfNodes(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), static_cast<double>(graph_->numberOfNodes()));
    }
    Napi::Value NumberOfEdges(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), static_cast<double>(graph_->numberOfEdges()));
    }
    Napi::Value UpperNodeIdBound(const Napi::CallbackInfo &info) {
        return Napi::Number::New(info.Env(), static_cast<double>(graph_->upperNodeIdBound()));
    }

    // ---- predicates ----
    Napi::Value HasNode(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) return Napi::Boolean::New(info.Env(), false);
        return Napi::Boolean::New(info.Env(), graph_->hasNode(jsToNode(info[0])));
    }
    Napi::Value HasEdge(const Napi::CallbackInfo &info) {
        if (info.Length() < 2) return Napi::Boolean::New(info.Env(), false);
        return Napi::Boolean::New(info.Env(),
                                  graph_->hasEdge(jsToNode(info[0]), jsToNode(info[1])));
    }
    Napi::Value IsWeighted(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), graph_->isWeighted());
    }
    Napi::Value IsDirected(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), graph_->isDirected());
    }
    Napi::Value IsEmpty(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), graph_->isEmpty());
    }

    // ---- per-node ----
    Napi::Value Degree(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "degree(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            return Napi::Number::New(info.Env(),
                                     static_cast<double>(graph_->degree(jsToNode(info[0]))));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value DegreeIn(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "degreeIn(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            return Napi::Number::New(info.Env(),
                                     static_cast<double>(graph_->degreeIn(jsToNode(info[0]))));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value Weight(const Napi::CallbackInfo &info) {
        if (info.Length() < 2) {
            Napi::TypeError::New(info.Env(), "weight(u, v)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        return Napi::Number::New(
            info.Env(),
            graph_->weight(jsToNode(info[0]), jsToNode(info[1])));
    }

    // ---- iteration ----
    Napi::Value Neighbors(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "neighbors(u)").ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            node u = jsToNode(info[0]);
            Napi::Array result = Napi::Array::New(info.Env());
            uint32_t idx = 0;
            graph_->forNeighborsOf(u, [&](node v) {
                result[idx++] = nodeToJs(info.Env(), v);
            });
            return result;
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value Nodes(const Napi::CallbackInfo &info) {
        Napi::Array result = Napi::Array::New(info.Env());
        uint32_t idx = 0;
        graph_->forNodes([&](node u) {
            result[idx++] = nodeToJs(info.Env(), u);
        });
        return result;
    }
    Napi::Value Edges(const Napi::CallbackInfo &info) {
        Napi::Array result = Napi::Array::New(info.Env());
        uint32_t idx = 0;
        graph_->forEdges([&](node u, node v) {
            Napi::Array pair = Napi::Array::New(info.Env(), 2);
            pair[0u] = nodeToJs(info.Env(), u);
            pair[1u] = nodeToJs(info.Env(), v);
            result[idx++] = pair;
        });
        return result;
    }
};

// ============================================================================
// Init registration  (must come after class bodies since we reference members)
// ============================================================================

Napi::Object GraphWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Graph", {
        InstanceMethod("numberOfNodes",   &GraphWrapper::NumberOfNodes),
        InstanceMethod("numberOfEdges",   &GraphWrapper::NumberOfEdges),
        InstanceMethod("upperNodeIdBound",&GraphWrapper::UpperNodeIdBound),
        InstanceMethod("addNode",         &GraphWrapper::AddNode),
        InstanceMethod("addEdge",         &GraphWrapper::AddEdge),
        InstanceMethod("removeNode",      &GraphWrapper::RemoveNode),
        InstanceMethod("removeEdge",      &GraphWrapper::RemoveEdge),
        InstanceMethod("setWeight",       &GraphWrapper::SetWeight),
        InstanceMethod("hasNode",         &GraphWrapper::HasNode),
        InstanceMethod("hasEdge",         &GraphWrapper::HasEdge),
        InstanceMethod("isWeighted",      &GraphWrapper::IsWeighted),
        InstanceMethod("isDirected",      &GraphWrapper::IsDirected),
        InstanceMethod("isEmpty",         &GraphWrapper::IsEmpty),
        InstanceMethod("degree",          &GraphWrapper::Degree),
        InstanceMethod("degreeIn",        &GraphWrapper::DegreeIn),
        InstanceMethod("weight",          &GraphWrapper::Weight),
        InstanceMethod("neighbors",       &GraphWrapper::Neighbors),
        InstanceMethod("nodes",           &GraphWrapper::Nodes),
        InstanceMethod("edges",           &GraphWrapper::Edges),
        InstanceMethod("weightedEdges",   &GraphWrapper::WeightedEdges),
    });
    g_GraphCtor = Napi::Persistent(func);
    g_GraphCtor.SuppressDestruct();
    exports.Set("Graph", func);
    return exports;
}

Napi::Object GraphRWrapper::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "GraphR", {
        InstanceMethod("numberOfNodes",   &GraphRWrapper::NumberOfNodes),
        InstanceMethod("numberOfEdges",   &GraphRWrapper::NumberOfEdges),
        InstanceMethod("upperNodeIdBound",&GraphRWrapper::UpperNodeIdBound),
        InstanceMethod("hasNode",         &GraphRWrapper::HasNode),
        InstanceMethod("hasEdge",         &GraphRWrapper::HasEdge),
        InstanceMethod("isWeighted",      &GraphRWrapper::IsWeighted),
        InstanceMethod("isDirected",      &GraphRWrapper::IsDirected),
        InstanceMethod("isEmpty",         &GraphRWrapper::IsEmpty),
        InstanceMethod("degree",          &GraphRWrapper::Degree),
        InstanceMethod("degreeIn",        &GraphRWrapper::DegreeIn),
        InstanceMethod("weight",          &GraphRWrapper::Weight),
        InstanceMethod("neighbors",       &GraphRWrapper::Neighbors),
        InstanceMethod("nodes",           &GraphRWrapper::Nodes),
        InstanceMethod("edges",           &GraphRWrapper::Edges),
    });
    g_GraphRCtor = Napi::Persistent(func);
    g_GraphRCtor.SuppressDestruct();
    exports.Set("GraphR", func);
    return exports;
}

// ============================================================================
// Helper: extract const Graph& from a JS object (Graph or GraphR).
// ============================================================================
static const Graph &extractGraph(const Napi::Value &val) {
    if (!val.IsObject())
        throw std::runtime_error("Expected a Graph or GraphR instance");
    Napi::Object obj = val.As<Napi::Object>();
    if (obj.InstanceOf(g_GraphCtor.Value()))
        return GraphWrapper::Unwrap(obj)->baseGraph();
    if (obj.InstanceOf(g_GraphRCtor.Value()))
        return GraphRWrapper::Unwrap(obj)->baseGraph();
    throw std::runtime_error("Expected a Graph or GraphR instance");
}

// ============================================================================
// Macro to generate the common Centrality wrapper boilerplate:
//   scores() → Float64Array
//   score(v) → number
//   ranking() → [{node, score}]
//   hasFinished() → boolean
// ============================================================================
#define CENTRALITY_COMMON_METHODS(AlgoClass)                                       \
    Napi::Value Run(const Napi::CallbackInfo &info) {                              \
        try { algo_->run(); }                                                      \
        catch (const std::exception &e) { throwError(info.Env(), e); }            \
        return info.Env().Undefined();                                             \
    }                                                                              \
    Napi::Value Scores(const Napi::CallbackInfo &info) {                           \
        try {                                                                      \
            algo_->assureFinished();                                               \
            const auto &s = algo_->scores();                                       \
            auto buf = Napi::ArrayBuffer::New(info.Env(),                          \
                           s.size() * sizeof(double));                             \
            std::memcpy(buf.Data(), s.data(), s.size() * sizeof(double));          \
            return Napi::Float64Array::New(info.Env(), s.size(), buf, 0);          \
        } catch (const std::exception &e) {                                        \
            throwError(info.Env(), e);                                             \
            return info.Env().Undefined();                                         \
        }                                                                          \
    }                                                                              \
    Napi::Value Score(const Napi::CallbackInfo &info) {                            \
        if (info.Length() < 1) {                                                   \
            Napi::TypeError::New(info.Env(), "score(v)")                           \
                .ThrowAsJavaScriptException();                                     \
            return info.Env().Undefined();                                         \
        }                                                                          \
        try {                                                                      \
            return Napi::Number::New(info.Env(),                                   \
                       algo_->score(jsToNode(info[0])));                           \
        } catch (const std::exception &e) {                                        \
            throwError(info.Env(), e);                                             \
            return info.Env().Undefined();                                         \
        }                                                                          \
    }                                                                              \
    Napi::Value Ranking(const Napi::CallbackInfo &info) {                          \
        try {                                                                      \
            auto ranking = algo_->ranking();                                       \
            Napi::Array arr = Napi::Array::New(info.Env(), ranking.size());        \
            for (uint32_t i = 0; i < (uint32_t)ranking.size(); ++i) {             \
                Napi::Object entry = Napi::Object::New(info.Env());                \
                entry.Set("node", nodeToJs(info.Env(), ranking[i].first));         \
                entry.Set("score", Napi::Number::New(info.Env(),                   \
                                       ranking[i].second));                        \
                arr[i] = entry;                                                    \
            }                                                                      \
            return arr;                                                            \
        } catch (const std::exception &e) {                                        \
            throwError(info.Env(), e);                                             \
            return info.Env().Undefined();                                         \
        }                                                                          \
    }                                                                              \
    Napi::Value HasFinished(const Napi::CallbackInfo &info) {                      \
        return Napi::Boolean::New(info.Env(), algo_->hasFinished());               \
    }

// ============================================================================
// BetweennessWrapper  (JS class: Betweenness)
// ============================================================================
class BetweennessWrapper : public Napi::ObjectWrap<BetweennessWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "Betweenness", {
            InstanceMethod("run",         &BetweennessWrapper::Run),
            InstanceMethod("scores",      &BetweennessWrapper::Scores),
            InstanceMethod("score",       &BetweennessWrapper::Score),
            InstanceMethod("ranking",     &BetweennessWrapper::Ranking),
            InstanceMethod("hasFinished", &BetweennessWrapper::HasFinished),
            InstanceMethod("maximum",     &BetweennessWrapper::Maximum),
        });
        exports.Set("Betweenness", func);
        return exports;
    }

    /**
     * JS: new Betweenness(graph, normalized=false, computeEdgeCentrality=false)
     */
    explicit BetweennessWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<BetweennessWrapper>(info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "Betweenness(graph [, normalized, computeEdgeCentrality])")
                .ThrowAsJavaScriptException();
            return;
        }
        try {
            bool normalized           = info.Length() >= 2 && info[1].As<Napi::Boolean>();
            bool computeEdgeCentrality= info.Length() >= 3 && info[2].As<Napi::Boolean>();
            graphRef_  = Napi::Persistent(info[0].As<Napi::Object>());
            const Graph &G = extractGraph(info[0]);
            algo_ = std::make_unique<Betweenness>(G, normalized, computeEdgeCentrality);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
    }

private:
    Napi::ObjectReference graphRef_;
    std::unique_ptr<Betweenness> algo_;

    CENTRALITY_COMMON_METHODS(Betweenness)

    Napi::Value Maximum(const Napi::CallbackInfo &info) {
        try {
            return Napi::Number::New(info.Env(), algo_->maximum());
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
};

// ============================================================================
// PageRankWrapper  (JS class: PageRank)
// ============================================================================
class PageRankWrapper : public Napi::ObjectWrap<PageRankWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "PageRank", {
            InstanceMethod("run",              &PageRankWrapper::Run),
            InstanceMethod("scores",           &PageRankWrapper::Scores),
            InstanceMethod("score",            &PageRankWrapper::Score),
            InstanceMethod("ranking",          &PageRankWrapper::Ranking),
            InstanceMethod("hasFinished",      &PageRankWrapper::HasFinished),
            InstanceMethod("numberOfIterations",&PageRankWrapper::NumberOfIterations),
        });
        exports.Set("PageRank", func);
        return exports;
    }

    /**
     * JS: new PageRank(graph, damp=0.85, tol=1e-8, normalized=false)
     */
    explicit PageRankWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<PageRankWrapper>(info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "PageRank(graph [, damp, tol, normalized])")
                .ThrowAsJavaScriptException();
            return;
        }
        try {
            double damp       = info.Length() >= 2 ? info[1].As<Napi::Number>().DoubleValue() : 0.85;
            double tol        = info.Length() >= 3 ? info[2].As<Napi::Number>().DoubleValue() : 1e-8;
            bool   normalized = info.Length() >= 4 && info[3].As<Napi::Boolean>();
            graphRef_ = Napi::Persistent(info[0].As<Napi::Object>());
            const Graph &G = extractGraph(info[0]);
            algo_ = std::make_unique<PageRank>(G, damp, tol, normalized);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
    }

private:
    Napi::ObjectReference graphRef_;
    std::unique_ptr<PageRank> algo_;

    CENTRALITY_COMMON_METHODS(PageRank)

    Napi::Value NumberOfIterations(const Napi::CallbackInfo &info) {
        try {
            return Napi::Number::New(info.Env(),
                static_cast<double>(algo_->numberOfIterations()));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
};

// ============================================================================
// DegreeCentralityWrapper  (JS class: DegreeCentrality)
// ============================================================================
class DegreeCentralityWrapper : public Napi::ObjectWrap<DegreeCentralityWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "DegreeCentrality", {
            InstanceMethod("run",         &DegreeCentralityWrapper::Run),
            InstanceMethod("scores",      &DegreeCentralityWrapper::Scores),
            InstanceMethod("score",       &DegreeCentralityWrapper::Score),
            InstanceMethod("ranking",     &DegreeCentralityWrapper::Ranking),
            InstanceMethod("hasFinished", &DegreeCentralityWrapper::HasFinished),
        });
        exports.Set("DegreeCentrality", func);
        return exports;
    }

    /**
     * JS: new DegreeCentrality(graph, normalized=false, outDeg=true, ignoreSelfLoops=true)
     */
    explicit DegreeCentralityWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<DegreeCentralityWrapper>(info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(),
                "DegreeCentrality(graph [, normalized, outDeg, ignoreSelfLoops])")
                .ThrowAsJavaScriptException();
            return;
        }
        try {
            bool normalized      = info.Length() >= 2 && info[1].As<Napi::Boolean>();
            bool outDeg          = info.Length() >= 3 ? (bool)info[2].As<Napi::Boolean>() : true;
            bool ignoreSelfLoops = info.Length() >= 4 ? (bool)info[3].As<Napi::Boolean>() : true;
            graphRef_ = Napi::Persistent(info[0].As<Napi::Object>());
            const Graph &G = extractGraph(info[0]);
            algo_ = std::make_unique<DegreeCentrality>(G, normalized, outDeg, ignoreSelfLoops);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
    }

private:
    Napi::ObjectReference graphRef_;
    std::unique_ptr<DegreeCentrality> algo_;

    CENTRALITY_COMMON_METHODS(DegreeCentrality)
};

// ============================================================================
// ConnectedComponentsWrapper  (JS class: ConnectedComponents)
// ============================================================================
class ConnectedComponentsWrapper : public Napi::ObjectWrap<ConnectedComponentsWrapper> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "ConnectedComponents", {
            InstanceMethod("run",                &ConnectedComponentsWrapper::Run),
            InstanceMethod("hasFinished",        &ConnectedComponentsWrapper::HasFinished),
            InstanceMethod("numberOfComponents", &ConnectedComponentsWrapper::NumberOfComponents),
            InstanceMethod("componentOfNode",    &ConnectedComponentsWrapper::ComponentOfNode),
            InstanceMethod("getComponents",      &ConnectedComponentsWrapper::GetComponents),
            InstanceMethod("getComponentSizes",  &ConnectedComponentsWrapper::GetComponentSizes),
        });
        exports.Set("ConnectedComponents", func);
        return exports;
    }

    /** JS: new ConnectedComponents(graph) */
    explicit ConnectedComponentsWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<ConnectedComponentsWrapper>(info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "ConnectedComponents(graph)")
                .ThrowAsJavaScriptException();
            return;
        }
        try {
            graphRef_ = Napi::Persistent(info[0].As<Napi::Object>());
            const Graph &G = extractGraph(info[0]);
            algo_ = std::make_unique<ConnectedComponents>(G);
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
        }
    }

private:
    Napi::ObjectReference graphRef_;
    std::unique_ptr<ConnectedComponents> algo_;

    Napi::Value Run(const Napi::CallbackInfo &info) {
        try { algo_->run(); }
        catch (const std::exception &e) { throwError(info.Env(), e); }
        return info.Env().Undefined();
    }
    Napi::Value HasFinished(const Napi::CallbackInfo &info) {
        return Napi::Boolean::New(info.Env(), algo_->hasFinished());
    }
    Napi::Value NumberOfComponents(const Napi::CallbackInfo &info) {
        try {
            return Napi::Number::New(info.Env(),
                static_cast<double>(algo_->numberOfComponents()));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    Napi::Value ComponentOfNode(const Napi::CallbackInfo &info) {
        if (info.Length() < 1) {
            Napi::TypeError::New(info.Env(), "componentOfNode(u)")
                .ThrowAsJavaScriptException();
            return info.Env().Undefined();
        }
        try {
            return Napi::Number::New(info.Env(),
                static_cast<double>(algo_->componentOfNode(jsToNode(info[0]))));
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    /** Returns array of arrays: [[node, ...], [node, ...], ...] */
    Napi::Value GetComponents(const Napi::CallbackInfo &info) {
        try {
            auto comps = algo_->getComponents();
            Napi::Array outer = Napi::Array::New(info.Env(), comps.size());
            for (uint32_t ci = 0; ci < (uint32_t)comps.size(); ++ci) {
                Napi::Array inner = Napi::Array::New(info.Env(), comps[ci].size());
                for (uint32_t ni = 0; ni < (uint32_t)comps[ci].size(); ++ni)
                    inner[ni] = nodeToJs(info.Env(), comps[ci][ni]);
                outer[ci] = inner;
            }
            return outer;
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
    /** Returns object: { componentId: size, ... } */
    Napi::Value GetComponentSizes(const Napi::CallbackInfo &info) {
        try {
            auto sizes = algo_->getComponentSizes();
            Napi::Object obj = Napi::Object::New(info.Env());
            for (auto &kv : sizes)
                obj.Set(std::to_string(kv.first),
                        Napi::Number::New(info.Env(), static_cast<double>(kv.second)));
            return obj;
        } catch (const std::exception &e) {
            throwError(info.Env(), e);
            return info.Env().Undefined();
        }
    }
};

// ============================================================================
// Free functions: readMETIS, readEdgeList
// ============================================================================

/**
 * JS: readMETIS(path) → Graph
 */
static Napi::Value ReadMETIS(const Napi::CallbackInfo &info) {
    if (info.Length() < 1) {
        Napi::TypeError::New(info.Env(), "readMETIS(path)").ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }
    try {
        std::string path = info[0].As<Napi::String>().Utf8Value();
        METISGraphReader reader;
        // reader returns a GraphW by value; move it onto the heap
        auto *gw = new GraphW(reader.read(path));
        auto ext = Napi::External<GraphW>::New(info.Env(), gw);
        return g_GraphCtor.New({ext});
    } catch (const std::exception &e) {
        throwError(info.Env(), e);
        return info.Env().Undefined();
    }
}

/**
 * JS: readEdgeList(path, separator=' ', firstNode=0, directed=false, commentPrefix='#') → Graph
 */
static Napi::Value ReadEdgeList(const Napi::CallbackInfo &info) {
    if (info.Length() < 1) {
        Napi::TypeError::New(info.Env(),
            "readEdgeList(path [, separator, firstNode, directed, commentPrefix])")
            .ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }
    try {
        std::string path   = info[0].As<Napi::String>().Utf8Value();
        char sep           = info.Length() >= 2
                             ? info[1].As<Napi::String>().Utf8Value()[0]
                             : ' ';
        node firstNode     = info.Length() >= 3
                             ? static_cast<node>(info[2].As<Napi::Number>().DoubleValue())
                             : 0;
        bool directed      = info.Length() >= 4 && info[3].As<Napi::Boolean>();
        std::string cmt    = info.Length() >= 5
                             ? info[4].As<Napi::String>().Utf8Value()
                             : "#";

        EdgeListReader reader(sep, firstNode, cmt, /*continuous=*/true, directed);
        auto *gw = new GraphW(reader.read(path));
        auto ext = Napi::External<GraphW>::New(info.Env(), gw);
        return g_GraphCtor.New({ext});
    } catch (const std::exception &e) {
        throwError(info.Env(), e);
        return info.Env().Undefined();
    }
}

// ============================================================================
// Module entry point
// ============================================================================
static Napi::Object InitModule(Napi::Env env, Napi::Object exports) {
    // Order matters: Graph and GraphR must be initialised before algorithm
    // wrappers so that g_GraphCtor / g_GraphRCtor are set when extractGraph()
    // is called inside algorithm constructors.
    GraphWrapper::Init(env, exports);
    GraphRWrapper::Init(env, exports);
    BetweennessWrapper::Init(env, exports);
    PageRankWrapper::Init(env, exports);
    DegreeCentralityWrapper::Init(env, exports);
    ConnectedComponentsWrapper::Init(env, exports);

    exports.Set("readMETIS",    Napi::Function::New(env, ReadMETIS));
    exports.Set("readEdgeList", Napi::Function::New(env, ReadEdgeList));

    return exports;
}

NODE_API_MODULE(icebug, InitModule)
