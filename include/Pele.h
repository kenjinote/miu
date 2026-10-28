#pragma once
#ifdef _MSC_VER
#pragma optimize("s", on)
#endif
// Pele C++ Native Port - 1:1 Zero-dependency Mermaid-to-SVG rendering engine
// Faithfully ported from Pele (https://pele.run / MIT License)

#pragma push_macro("min")
#pragma push_macro("max")
#pragma push_macro("small")
#pragma push_macro("near")
#pragma push_macro("far")
#pragma push_macro("interface")
#pragma push_macro("IN")
#pragma push_macro("OUT")
#pragma push_macro("ERROR")
#pragma push_macro("ABSOLUTE")
#pragma push_macro("RELATIVE")
#pragma push_macro("TRANSPARENT")
#pragma push_macro("OPAQUE")
#pragma push_macro("OPTIONAL")
#pragma push_macro("NEAR")
#pragma push_macro("FAR")
#pragma push_macro("CALLBACK")
#pragma push_macro("PASSTHROUGH")
#pragma push_macro("DIFFERENCE")
#pragma push_macro("ALTERNATE")
#pragma push_macro("WINDING")
#pragma push_macro("DELETE")
#undef min
#undef max
#undef small
#undef near
#undef far
#undef interface
#undef IN
#undef OUT
#undef ERROR
#undef ABSOLUTE
#undef RELATIVE
#undef TRANSPARENT
#undef OPAQUE
#undef OPTIONAL
#undef NEAR
#undef FAR
#undef CALLBACK
#undef PASSTHROUGH
#undef DIFFERENCE
#undef ALTERNATE
#undef WINDING
#undef DELETE

#include "pele/foundation.h"

#include "pele/diagrams/flowchart.h"
#include "pele/diagrams/swimlane.h"
#include "pele/diagrams/usecase.h"
#include "pele/diagrams/agentflow.h"
#include "pele/diagrams/block.h"
#include "pele/diagrams/c4.h"
#include "pele/diagrams/state.h"
#include "pele/diagrams/class.h"
#include "pele/diagrams/er.h"
#include "pele/diagrams/sequence.h"
#include "pele/diagrams/venn.h"
#include "pele/diagrams/xychart.h"
#include "pele/diagrams/wardley.h"
#include "pele/diagrams/railroad.h"
#include "pele/diagrams/gantt.h"
#include "pele/diagrams/architecture.h"
#include "pele/diagrams/requirement.h"
#include "pele/diagrams/radar.h"
#include "pele/diagrams/treemap.h"
#include "pele/diagrams/cynefin.h"
#include "pele/diagrams/treeview.h"
#include "pele/diagrams/eventmodeling.h"
#include "pele/diagrams/sankey.h"
#include "pele/diagrams/git.h"
#include "pele/diagrams/quadrant.h"
#include "pele/diagrams/info.h"
#include "pele/diagrams/pie.h"
#include "pele/diagrams/packet.h"
#include "pele/diagrams/mindmap.h"
#include "pele/diagrams/kanban.h"
#include "pele/diagrams/ishikawa.h"
#include "pele/diagrams/timeline.h"
#include "pele/diagrams/journey.h"

namespace Pele {

struct RenderResult {
    bool ok = false;
    std::string svg;
    std::string type;
    std::string mermaidType;
    double width = 0.0;
    double height = 0.0;
    std::vector<LinkInfo> links;
    std::string error;
};

inline MathMarkupResolver defaultMathMarkupResolver() {
    return [](const std::string& tex, double fontSize, MathMarkup& out) -> bool {
        MathBox box = detail::typesetLatexMath(tex, fontSize, true);
        if (box.width <= 0.0 || box.height <= 0.0 || (box.svg.empty() && box.html.empty())) return false;
        out.svg = std::move(box.svg);
        out.html = std::move(box.html);
        out.width = box.width;
        out.height = box.height;
        return true;
    };
}

inline RenderResult render(const std::string& text, const RenderOptions& options = RenderOptions()) {
    RenderResult result;
    try {
        constexpr size_t DEFAULT_LIMIT = 50000;
        constexpr size_t DEFAULT_OUTPUT_LIMIT = 4000000;
        if (text.size() > DEFAULT_LIMIT) {
            throw PeleError("Diagram source is longer than the limit of " + std::to_string(DEFAULT_LIMIT) + " characters.", "limit");
        }

        Preprocessed pre = preprocess(text);
        Detected found;
        if (!detectDiagram(pre.text, found)) {
            throw PeleError("No diagram type detected for the given text.", "unsupported-diagram");
        }

        const std::string& dtype = found.type;
        bool keepComments = (dtype == "agentflow" || dtype == "eventmodeling");
        bool hasMath = (
            dtype == "block" || dtype == "class" || dtype == "er" || dtype == "flowchart" ||
            dtype == "kanban" || dtype == "mindmap" || dtype == "requirement" ||
            dtype == "sequence" || dtype == "state" || dtype == "swimlane" || dtype == "usecase"
        );

        std::string sectionKey = dtype;
        if (dtype == "wardley") sectionKey = "wardley-beta";
        else if (dtype == "xychart") sectionKey = "xyChart";

        bool fixed = false;
        if (pre.config.isMap()) {
            const Config* sec = pre.config.get(sectionKey);
            if (sec && sec->isMap()) {
                const Config* umw = sec->get("useMaxWidth");
                if (umw && umw->isBool() && !umw->bVal) fixed = true;
            }
        }

        RenderOptions opts = options;
        if (!opts.hasResponsive) {
            opts.responsive = !fixed;
            opts.hasResponsive = true;
        }
        IconResolver rawIcons = opts.icons;
        IconResolver wrappedIcons = nullptr;
        if (rawIcons) {
            wrappedIcons = [rawIcons](const std::string& name) {
                return rawIcons(decodeEntities(name));
            };
        }
        opts.icons = wrappedIcons;

        MathMarkupResolver mathRes = nullptr;
        if (hasMath) {
            mathRes = opts.math ? opts.math : defaultMathMarkupResolver();
        }
        opts.math = mathRes;

        std::string parsed = encodeEntities(keepComments ? pre.withComments : pre.text) + "\n";

        Rendered rendered = withMath(mathRes, [&]() -> Rendered {
            return withIcons(wrappedIcons, [&]() -> Rendered {
                if (dtype == "flowchart") return Flowchart::render(parsed, pre.config, pre.title, opts);
                if (dtype == "swimlane") return Swimlane::render(parsed, pre.config, pre.title, opts);
                if (dtype == "usecase") return Usecase::render(parsed, pre.config, pre.title, opts);
                if (dtype == "agentflow") return renderAgentflow(text, opts);
                if (dtype == "block") return BlockDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "c4") return C4Diagram::render(parsed, pre.config, pre.title, opts, pre.lineOffset);
                if (dtype == "state") return StateDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "class") return ClassDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "er") return ErDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "sequence") return SequenceDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "venn") return VennDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "xychart") return renderXYChart(parsed, pre.config, pre.title, opts);
                if (dtype == "wardley") return WardleyDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "railroad") return RailroadDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "gantt") return GanttDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "architecture") return ArchitectureDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "requirement") return RequirementDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "radar") return RadarDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "treemap") return TreemapDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "cynefin") return CynefinDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "treeView") return TreeViewDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "eventmodeling") return EventModelingDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "sankey") return SankeyDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "gitGraph") return GitDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "quadrantChart") return QuadrantDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "info") return InfoDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "pie") return PieDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "packet") return PacketDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "mindmap") return MindmapDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "kanban") return KanbanDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "ishikawa") return IshikawaDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "timeline") return TimelineDiagram::render(parsed, pre.config, pre.title, opts);
                if (dtype == "journey") return JourneyDiagram::render(parsed, pre.config, pre.title, opts);
                throw PeleError("Diagram type \"" + dtype + "\" is not supported yet.", "unsupported-diagram");
            });
        });

        if (rendered.svg.size() > DEFAULT_OUTPUT_LIMIT) {
            throw PeleError("Diagram output is longer than the limit of " + std::to_string(DEFAULT_OUTPUT_LIMIT) + " characters.", "limit");
        }
        for (auto& link : rendered.links) {
            link.id = decodeEntities(link.id);
        }

        result.ok = !rendered.svg.empty();
        result.svg = std::move(rendered.svg);
        result.type = found.type;
        result.mermaidType = found.mermaidType;
        result.width = rendered.width;
        result.height = rendered.height;
        result.links = std::move(rendered.links);
    } catch (const std::exception& ex) {
        result.ok = false;
        result.error = ex.what();
    }
    return result;
}

} // namespace Pele

#pragma pop_macro("DELETE")
#pragma pop_macro("WINDING")
#pragma pop_macro("ALTERNATE")
#pragma pop_macro("DIFFERENCE")
#pragma pop_macro("PASSTHROUGH")
#pragma pop_macro("CALLBACK")
#pragma pop_macro("FAR")
#pragma pop_macro("NEAR")
#pragma pop_macro("OPTIONAL")
#pragma pop_macro("OPAQUE")
#pragma pop_macro("TRANSPARENT")
#pragma pop_macro("RELATIVE")
#pragma pop_macro("ABSOLUTE")
#pragma pop_macro("ERROR")
#pragma pop_macro("OUT")
#pragma pop_macro("IN")
#pragma pop_macro("interface")
#pragma pop_macro("far")
#pragma pop_macro("near")
#pragma pop_macro("small")
#pragma pop_macro("max")
#pragma pop_macro("min")
#ifdef _MSC_VER
#pragma optimize("", on)
#endif
