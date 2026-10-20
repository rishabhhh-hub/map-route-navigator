// Map Route Navigator - frontend logic.
//
// RESPONSIBILITIES OF THIS FILE
//   * fetch the graph from the C++ backend (GET /api/graph) and draw it
//   * let the user pick a start / destination / algorithm
//   * send route requests to the C++ backend (POST /api/route)
//   * display and animate whatever the backend answers
//
// WHAT THIS FILE DELIBERATELY DOES NOT DO
//   * It does not implement Dijkstra, A*, a priority queue or any other
//     pathfinding. Every distance, path, explored-node list and timing shown
//     on the page comes straight from a backend response.
//   * It does not contain a copy of the graph. The graph is fetched.
//   * setTimeout is used only to pace the visual animation.

(() => {
  'use strict';

  const SVG_NS = 'http://www.w3.org/2000/svg';
  const ALGORITHM_LABELS = { dijkstra: 'Dijkstra', astar: 'A*' };

  // ---------------------------------------------------------------------
  // State
  // ---------------------------------------------------------------------
  const state = {
    nodes: [],              // [{id, name, x, y}]            from GET /api/graph
    edges: [],              // [{from, to, weight, bidirectional}]
    nodeById: new Map(),
    startId: null,
    destId: null,
    busy: false,
    animationToken: 0,      // bumping this cancels a running animation
    graphLoaded: false,
  };

  const nodeEls = new Map();  // node id -> <g class="node">
  const edgeEls = [];         // [{ edge, group }]

  // ---------------------------------------------------------------------
  // DOM references
  // ---------------------------------------------------------------------
  const $ = (id) => document.getElementById(id);
  const el = {
    status: $('backend-status'),
    startSelect: $('start-select'),
    destSelect: $('dest-select'),
    algoSelect: $('algo-select'),
    findBtn: $('find-btn'),
    compareBtn: $('compare-btn'),
    resetBtn: $('reset-btn'),
    showWeights: $('show-weights'),
    banner: $('error-banner'),
    map: $('map'),
    mapWrap: $('map-wrap'),
    loading: $('loading'),
    resultsEmpty: $('results-empty'),
    resultsList: $('results-list'),
    resAlgorithm: $('res-algorithm'),
    resReachable: $('res-reachable'),
    resDistance: $('res-distance'),
    resExplored: $('res-explored'),
    resTime: $('res-time'),
    resPath: $('res-path'),
    resOrder: $('res-order'),
    comparePanel: $('comparison-panel'),
    compareCards: $('compare-cards'),
    compareSummary: $('compare-summary'),
  };

  // ---------------------------------------------------------------------
  // Small helpers
  // ---------------------------------------------------------------------
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const prefersReducedMotion = () =>
    window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  function svgEl(name, attrs, text) {
    const node = document.createElementNS(SVG_NS, name);
    Object.keys(attrs || {}).forEach((key) => node.setAttribute(key, String(attrs[key])));
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function htmlEl(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function formatNumber(value) {
    return Number.isInteger(value) ? String(value) : value.toFixed(2);
  }

  function formatMs(ms) {
    return ms < 0.0001 ? '< 0.0001 ms' : ms.toFixed(4) + ' ms';
  }

  function nodeName(id) {
    const node = state.nodeById.get(id);
    return node ? node.name : String(id);
  }

  function describePath(path) {
    return path.map((id) => id + ' ' + nodeName(id)).join('  →  ');
  }

  // ---------------------------------------------------------------------
  // Talking to the C++ backend
  // ---------------------------------------------------------------------
  const NETWORK_HELP =
    'Cannot reach the C++ backend. Start the server (map_route_navigator) and open ' +
    'http://127.0.0.1:8080 in your browser - do not open index.html as a file.';

  async function requestJson(url) {
    let response;
    try {
      response = await fetch(url);
    } catch (err) {
      throw new Error(NETWORK_HELP);
    }
    let data = null;
    try {
      data = await response.json();
    } catch (err) {
      data = null;
    }
    return { status: response.status, ok: response.ok, data };
  }

  // POST /api/route - the C++ backend runs the algorithm and answers.
  async function requestRoute(startNodeId, destinationNodeId, selectedAlgorithm) {
    let response;
    try {
      response = await fetch('/api/route', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          start: startNodeId,
          destination: destinationNodeId,
          algorithm: selectedAlgorithm,
        }),
      });
    } catch (err) {
      throw new Error(NETWORK_HELP);
    }

    let result = null;
    try {
      result = await response.json();
    } catch (err) {
      result = null;
    }

    // 200: route found.
    if (response.status === 200 && result) return result;

    // 404 + reachable:false: the request was valid but no road connects the
    // two nodes. The body still carries the full (unreachable) result.
    if (response.status === 404 && result && result.reachable === false) return result;

    const message = result && result.message ? result.message : 'HTTP ' + response.status;
    throw new Error('Backend error: ' + message);
  }

  // ---------------------------------------------------------------------
  // Messages and busy state
  // ---------------------------------------------------------------------
  function showMessage(text, kind) {
    el.banner.textContent = text;
    el.banner.className = 'banner ' + (kind === 'info' ? 'banner-info' : 'banner-error');
    el.banner.hidden = false;
  }

  function hideMessage() {
    el.banner.hidden = true;
    el.banner.textContent = '';
  }

  function updateButtons() {
    const disabled = state.busy || !state.graphLoaded;
    el.findBtn.disabled = disabled;
    el.compareBtn.disabled = disabled;
    el.compareCards.querySelectorAll('button').forEach((b) => { b.disabled = state.busy; });
  }

  function setBusy(busy) {
    state.busy = busy;
    el.loading.hidden = !busy;
    el.mapWrap.setAttribute('aria-busy', String(busy));
    updateButtons();
  }

  // ---------------------------------------------------------------------
  // Drawing the graph (coordinates come from the backend)
  // ---------------------------------------------------------------------
  function renderGraph() {
    el.map.replaceChildren();
    nodeEls.clear();
    edgeEls.length = 0;

    const xs = state.nodes.map((n) => n.x);
    const ys = state.nodes.map((n) => n.y);
    const pad = 70;
    const minX = Math.min(...xs) - pad;
    const minY = Math.min(...ys) - pad;
    const width = Math.max(...xs) - Math.min(...xs) + 2 * pad;
    const height = Math.max(...ys) - Math.min(...ys) + 2 * pad;
    el.map.setAttribute('viewBox', minX + ' ' + minY + ' ' + width + ' ' + height);

    const edgeLayer = svgEl('g', { 'class': 'edges' });
    const nodeLayer = svgEl('g', { 'class': 'nodes' });
    el.map.append(edgeLayer, nodeLayer);

    // Roads
    state.edges.forEach((edge) => {
      const a = state.nodeById.get(edge.from);
      const b = state.nodeById.get(edge.to);
      if (!a || !b) return;
      const arrow = edge.bidirectional ? ' ↔ ' : ' → ';
      const group = svgEl('g', { 'class': 'edge' });
      group.append(
        svgEl('title', {}, a.name + arrow + b.name + ': weight ' + formatNumber(edge.weight)),
        svgEl('line', { 'class': 'edge-hit', x1: a.x, y1: a.y, x2: b.x, y2: b.y }),
        svgEl('line', { 'class': 'edge-line', x1: a.x, y1: a.y, x2: b.x, y2: b.y }),
        svgEl('text', { 'class': 'edge-weight', x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 },
          formatNumber(edge.weight))
      );
      edgeLayer.append(group);
      edgeEls.push({ edge, group });
    });

    // Intersections
    state.nodes.forEach((node) => {
      const baseTitle = node.id + ' - ' + node.name;
      const group = svgEl('g', {
        'class': 'node',
        tabindex: 0,
        role: 'button',
        'aria-label': 'Intersection ' + baseTitle,
      });
      group.dataset.baseTitle = baseTitle;
      group.append(
        svgEl('title', {}, baseTitle),
        svgEl('circle', { 'class': 'node-circle', cx: node.x, cy: node.y, r: 17 }),
        svgEl('text', { 'class': 'node-id', x: node.x, y: node.y }, String(node.id)),
        svgEl('text', { 'class': 'node-label', x: node.x, y: node.y + 36 }, node.name)
      );
      group.addEventListener('click', () => onNodeActivated(node.id));
      group.addEventListener('keydown', (event) => {
        if (event.key === 'Enter' || event.key === ' ') {
          event.preventDefault();
          onNodeActivated(node.id);
        }
      });
      nodeLayer.append(group);
      nodeEls.set(node.id, group);
    });
  }

  function populateSelects() {
    [[el.startSelect, 'Select start…'], [el.destSelect, 'Select destination…']].forEach(
      ([select, placeholder]) => {
        select.replaceChildren();
        const first = htmlEl('option', '', placeholder);
        first.value = '';
        select.append(first);
        state.nodes.forEach((node) => {
          const option = htmlEl('option', '', node.id + ' — ' + node.name);
          option.value = String(node.id);
          select.append(option);
        });
        select.disabled = false;
      }
    );
  }

  // ---------------------------------------------------------------------
  // Highlighting and animation (visual only)
  // ---------------------------------------------------------------------
  function cancelAnimation() {
    state.animationToken += 1;
  }

  function clearHighlights() {
    nodeEls.forEach((group) => {
      group.classList.remove('is-explored', 'is-path');
      group.querySelector('title').textContent = group.dataset.baseTitle;
    });
    edgeEls.forEach(({ group }) => group.classList.remove('is-path'));
  }

  function clearVisualization() {
    cancelAnimation();
    clearHighlights();
  }

  function applySelectionClasses() {
    nodeEls.forEach((group, id) => {
      group.classList.toggle('is-start', id === state.startId);
      group.classList.toggle('is-dest', id === state.destId);
    });
    el.startSelect.value = state.startId === null ? '' : String(state.startId);
    el.destSelect.value = state.destId === null ? '' : String(state.destId);
  }

  // Highlights the road used between two consecutive path nodes (the cheapest
  // matching road if there are several).
  function highlightRoad(fromId, toId) {
    let best = null;
    edgeEls.forEach((entry) => {
      const e = entry.edge;
      const forward = e.from === fromId && e.to === toId;
      const backward = e.bidirectional && e.from === toId && e.to === fromId;
      if ((forward || backward) && (best === null || e.weight < best.edge.weight)) best = entry;
    });
    if (best) best.group.classList.add('is-path');
  }

  // Replays a backend result: first the explored nodes in the order the C++
  // search settled them, then the final shortest path.
  async function animateResult(result) {
    cancelAnimation();
    const token = state.animationToken;
    clearHighlights();

    const reduced = prefersReducedMotion();
    const order = result.exploredOrder || [];
    const exploreDelay = reduced ? 0 : Math.min(160, 2000 / Math.max(order.length, 1));

    for (let i = 0; i < order.length; i += 1) {
      if (token !== state.animationToken) return;
      const group = nodeEls.get(order[i]);
      if (group) {
        group.classList.add('is-explored');
        group.querySelector('title').textContent =
          group.dataset.baseTitle + ' (explored #' + (i + 1) + ')';
      }
      if (exploreDelay > 0) await sleep(exploreDelay);
    }

    if (!result.reachable) return;

    const pathDelay = reduced ? 0 : 220;
    for (let i = 0; i < result.path.length; i += 1) {
      if (token !== state.animationToken) return;
      const group = nodeEls.get(result.path[i]);
      if (group) group.classList.add('is-path');
      if (i > 0) highlightRoad(result.path[i - 1], result.path[i]);
      if (pathDelay > 0) await sleep(pathDelay);
    }
  }

  // ---------------------------------------------------------------------
  // Results panel
  // ---------------------------------------------------------------------
  function resetResults() {
    el.resultsEmpty.hidden = false;
    el.resultsList.hidden = true;
  }

  function renderResult(result) {
    el.resultsEmpty.hidden = true;
    el.resultsList.hidden = false;

    el.resAlgorithm.textContent = ALGORITHM_LABELS[result.algorithm] || result.algorithm;
    el.resReachable.textContent = result.reachable ? 'Yes' : 'No — the destination is unreachable';
    el.resDistance.textContent = result.reachable ? formatNumber(result.distance) + ' units' : '—';
    el.resExplored.textContent = String(result.nodesExplored);
    el.resTime.textContent = formatMs(result.executionTimeMs);

    el.resPath.textContent = result.reachable ? describePath(result.path) : 'No route';
    el.resPath.classList.add('small');
    el.resOrder.textContent = result.exploredOrder.length
      ? result.exploredOrder.map((id) => id + ' ' + nodeName(id)).join(', ')
      : '—';
    el.resOrder.classList.add('small');
  }

  function showOnMap(result) {
    renderResult(result);
    return animateResult(result);
  }

  // ---------------------------------------------------------------------
  // Comparison panel
  // ---------------------------------------------------------------------
  function hideComparison() {
    el.comparePanel.hidden = true;
    el.compareCards.replaceChildren();
    el.compareSummary.replaceChildren();
  }

  function buildCompareCard(result) {
    const card = htmlEl('div', 'compare-card');
    card.append(htmlEl('h3', '', ALGORITHM_LABELS[result.algorithm] || result.algorithm));

    const table = htmlEl('table');
    const rows = [
      ['Path exists', result.reachable ? 'Yes' : 'No'],
      ['Distance', result.reachable ? formatNumber(result.distance) + ' units' : '—'],
      ['Nodes explored', String(result.nodesExplored)],
      ['Backend time', formatMs(result.executionTimeMs)],
      ['Path', result.reachable ? describePath(result.path) : 'No route'],
    ];
    rows.forEach(([label, value]) => {
      const tr = htmlEl('tr');
      tr.append(htmlEl('th', '', label), htmlEl('td', '', value));
      table.append(tr);
    });
    card.append(table);

    const button = htmlEl('button', 'btn', 'Show on map');
    button.type = 'button';
    button.addEventListener('click', () => { showOnMap(result); });
    card.append(button);
    return card;
  }

  function renderComparison(dijkstra, astar) {
    el.compareCards.replaceChildren(buildCompareCard(dijkstra), buildCompareCard(astar));

    const lines = [];
    if (dijkstra.reachable !== astar.reachable) {
      lines.push('Warning: the two algorithms disagree about whether a route exists.');
    } else if (!dijkstra.reachable) {
      lines.push('Both algorithms report that no route exists between these nodes.');
    } else if (Math.abs(dijkstra.distance - astar.distance) < 1e-9) {
      lines.push('Both algorithms found a shortest route of the same distance: ' +
        formatNumber(dijkstra.distance) + ' units.');
    } else {
      lines.push('Warning: the distances differ (Dijkstra ' + formatNumber(dijkstra.distance) +
        ', A* ' + formatNumber(astar.distance) + ').');
    }

    const diff = dijkstra.nodesExplored - astar.nodesExplored;
    let exploredText = 'Nodes explored: Dijkstra ' + dijkstra.nodesExplored + ', A* ' +
      astar.nodesExplored;
    if (diff > 0) exploredText += ' (A* explored ' + diff + ' fewer).';
    else if (diff < 0) exploredText += ' (A* explored ' + (-diff) + ' more).';
    else exploredText += ' (the same number).';
    lines.push(exploredText);

    lines.push('Backend time for this single run: Dijkstra ' + formatMs(dijkstra.executionTimeMs) +
      ', A* ' + formatMs(astar.executionTimeMs) + '.');

    el.compareSummary.replaceChildren(...lines.map((line) => htmlEl('div', '', line)));
    el.comparePanel.hidden = false;
    updateButtons();
  }

  // ---------------------------------------------------------------------
  // Selection handling
  // ---------------------------------------------------------------------
  function setSelection(startId, destId) {
    state.startId = startId;
    state.destId = destId;
    clearVisualization();
    resetResults();
    hideComparison();
    hideMessage();
    applySelectionClasses();
  }

  function onNodeActivated(id) {
    if (state.busy) return;
    if (state.startId === null) {
      setSelection(id, null);
    } else if (state.destId === null) {
      setSelection(state.startId, id);
    } else {
      setSelection(id, null);  // third click starts a new selection
    }
  }

  function parseSelect(select) {
    return select.value === '' ? null : Number(select.value);
  }

  function selectionIsComplete() {
    if (state.startId === null || state.destId === null) {
      showMessage('Please choose both a start and a destination (use the lists or click nodes on the map).', 'error');
      return false;
    }
    return true;
  }

  // ---------------------------------------------------------------------
  // Button handlers
  // ---------------------------------------------------------------------
  async function onFindRoute() {
    if (state.busy || !selectionIsComplete()) return;
    hideMessage();
    hideComparison();
    clearVisualization();
    setBusy(true);
    let result = null;
    try {
      result = await requestRoute(state.startId, state.destId, el.algoSelect.value);
    } catch (err) {
      showMessage(err.message, 'error');
    } finally {
      setBusy(false);
    }
    if (!result) return;

    if (!result.reachable) {
      showMessage(result.message || 'No route exists between the selected nodes.', 'info');
    }
    await showOnMap(result);
  }

  async function onCompare() {
    if (state.busy || !selectionIsComplete()) return;
    hideMessage();
    hideComparison();
    clearVisualization();
    setBusy(true);
    let dijkstra = null;
    let astar = null;
    try {
      // One after the other (not in parallel) so the two timings do not
      // compete for the same CPU.
      dijkstra = await requestRoute(state.startId, state.destId, 'dijkstra');
      astar = await requestRoute(state.startId, state.destId, 'astar');
    } catch (err) {
      showMessage(err.message, 'error');
    } finally {
      setBusy(false);
    }
    if (!dijkstra || !astar) return;

    renderComparison(dijkstra, astar);
    const shown = el.algoSelect.value === 'astar' ? astar : dijkstra;
    await showOnMap(shown);
  }

  function onReset() {
    setSelection(null, null);
  }

  // ---------------------------------------------------------------------
  // Start-up
  // ---------------------------------------------------------------------
  function setStatus(text, kind) {
    el.status.textContent = text;
    el.status.className = 'pill pill-' + kind;
  }

  async function init() {
    el.findBtn.addEventListener('click', onFindRoute);
    el.compareBtn.addEventListener('click', onCompare);
    el.resetBtn.addEventListener('click', onReset);
    el.startSelect.addEventListener('change', () => setSelection(parseSelect(el.startSelect), state.destId));
    el.destSelect.addEventListener('change', () => setSelection(state.startId, parseSelect(el.destSelect)));
    el.algoSelect.addEventListener('change', () => { clearVisualization(); resetResults(); hideComparison(); });
    el.showWeights.addEventListener('change', () => {
      el.map.classList.toggle('hide-weights', !el.showWeights.checked);
    });

    try {
      const [health, graph] = await Promise.all([requestJson('/api/health'), requestJson('/api/graph')]);
      if (!health.ok || !graph.ok || !graph.data) {
        throw new Error('The backend answered, but not with the expected data.');
      }

      state.nodes = graph.data.nodes;
      state.edges = graph.data.edges;
      state.nodeById = new Map(state.nodes.map((n) => [n.id, n]));
      state.graphLoaded = true;

      renderGraph();
      populateSelects();
      applySelectionClasses();
      updateButtons();
      setStatus('Backend online · API v' + health.data.apiVersion + ' · ' +
        state.nodes.length + ' nodes, ' + state.edges.length + ' roads', 'ok');
    } catch (err) {
      setStatus('Backend offline', 'bad');
      showMessage(err.message, 'error');
    }
  }

  init();
})();
