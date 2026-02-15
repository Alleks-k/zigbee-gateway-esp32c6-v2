import os
import re
import json
from pathlib import Path

ROOT = Path(".").resolve()
OUT_HTML = Path("docs/include_dependency_graph.html")

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')

# Directories to scan for include resolution
INCLUDE_DIRS = [
    ROOT,
    ROOT / "main",
    ROOT / "main" / "core",
    ROOT / "components",
]

def collect_files():
    files = []
    for p in ROOT.rglob("*"):
        if p.suffix in {".c", ".h"} and p.is_file():
            # ignore build and managed folders
            if any(part in ("build", "managed_components", ".git") for part in p.parts):
                continue
            files.append(p)
    return files

def resolve_include(include, origin):
    # First: relative to origin file
    candidate = (origin.parent / include).resolve()
    if candidate.exists():
        return candidate

    # Then: search in include dirs
    for base in INCLUDE_DIRS:
        candidate = (base / include).resolve()
        if candidate.exists():
            return candidate
    return None

def build_graph(files):
    nodes = []
    edges = []
    index = {}

    for f in files:
        rel = f.relative_to(ROOT).as_posix()
        idx = len(nodes)
        nodes.append({"id": idx, "label": rel})
        index[rel] = idx

    for f in files:
        origin_rel = f.relative_to(ROOT).as_posix()
        origin_id = index[origin_rel]
        with f.open("r", errors="ignore") as fh:
            for line in fh:
                m = INCLUDE_RE.match(line)
                if not m:
                    continue
                inc = m.group(1)
                target = resolve_include(inc, f)
                if target:
                    target_rel = target.relative_to(ROOT).as_posix()
                    if target_rel in index:
                        edges.append({"from": origin_id, "to": index[target_rel]})
    return nodes, edges

def render_html(nodes, edges):
    data = {"nodes": nodes, "edges": edges}
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<title>Include Dependency Graph</title>
<style>
  body {{ font-family: Arial, sans-serif; margin: 0; background:#f6f8fb; }}
  #graph {{ width: 100vw; height: 100vh; }}
  .controls {{ position: fixed; top: 10px; left: 10px; z-index: 10; background:#fff;
    border:1px solid #ddd; padding:8px; border-radius:8px; }}
  input {{ padding:4px 6px; }}
</style>
</head>
<body>
<div class="controls">
  <input id="search" placeholder="filter by file..." />
</div>
<div id="graph"></div>
<script src="https://unpkg.com/vis-network/standalone/umd/vis-network.min.js"></script>
<script>
const data = {json.dumps(data)};
const container = document.getElementById('graph');

let nodes = new vis.DataSet(data.nodes.map(n => ({{
  id: n.id, label: n.label, title: n.label
}})));
let edges = new vis.DataSet(data.edges.map(e => ({{
  from: e.from, to: e.to, arrows: 'to'
}})));

let network = new vis.Network(container, {{ nodes, edges }}, {{
  physics: {{ stabilization: false }},
  layout: {{ improvedLayout: true }}
}});

document.getElementById('search').addEventListener('input', (e) => {{
  const q = e.target.value.trim().toLowerCase();
  nodes.forEach(n => {{
    const visible = !q || n.label.toLowerCase().includes(q);
    nodes.update({{ id: n.id, hidden: !visible }});
  }});
}});
</script>
</body>
</html>"""

def main():
    files = collect_files()
    nodes, edges = build_graph(files)
    html = render_html(nodes, edges)
    OUT_HTML.parent.mkdir(parents=True, exist_ok=True)
    OUT_HTML.write_text(html, encoding="utf-8")
    print(f"✅ Generated {OUT_HTML} with {len(nodes)} nodes and {len(edges)} edges")

if __name__ == "__main__":
    main()