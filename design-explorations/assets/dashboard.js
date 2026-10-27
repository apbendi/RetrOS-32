/*
 * The OgOs directions dashboard: filters, the case switch, the shortlist and
 * the detail sheet. Directions come from OG.directions (assets/ogos.js and
 * directions/*.js, loaded before this file).
 */
(function () {
  'use strict';

  const $ = (sel, root = document) => root.querySelector(sel);
  const esc = OG.esc;
  const pad = (n) => String(n).padStart(2, '0');

  const ALL = OG.directions.slice().sort((a, b) => a.num - b.num);
  const byId = new Map(ALL.map((d) => [d.id, d]));
  ALL.forEach((d) => {
    const probs = OG.check(d);
    if (probs.length) console.warn(`#${d.num} ${d.name}: ${probs.join(', ')}`);
  });

  /* --------------------------------------------------------------- storage */

  // Browser storage may be missing or blocked; everything works without it.
  const store = {
    get(key, fallback) {
      try {
        const v = localStorage.getItem('ogos.' + key);
        return v == null ? fallback : JSON.parse(v);
      } catch (e) {
        return fallback;
      }
    },
    set(key, value) {
      try {
        localStorage.setItem('ogos.' + key, JSON.stringify(value));
      } catch (e) {
        /* not saved */
      }
    },
  };

  const prefs = Object.assign({ caseMode: 'design', ground: 'own', group: 'none', sort: 'num', size: 300 }, store.get('prefs', {}));
  const stars = new Set(store.get('stars', []).filter((id) => byId.has(id)));
  const notes = store.get('notes', {});
  const savePrefs = () => store.set('prefs', prefs);
  const saveStars = () => store.set('stars', [...stars]);

  /* ----------------------------------------------------------------- fonts */

  // One stylesheet per family, so a family Google doesn't know only loses itself.
  const specs = new Set();
  ALL.forEach((d) => (d.fonts || []).forEach((f) => specs.add(String(f).trim())));
  specs.forEach((spec) => {
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'https://fonts.googleapis.com/css2?family=' + spec.replace(/ /g, '+') + '&display=swap';
    document.head.appendChild(link);
  });

  /* ------------------------------------------------------------- rendering */

  const GROUNDS = {
    own: { label: 'Own', color: null },
    light: { label: 'Light', color: '#F5F5F2' },
    dark: { label: 'Dark', color: '#131316' },
  };
  const groundFor = (d) => GROUNDS[prefs.ground].color || d.bg;
  const wordFor = (d, mode = prefs.caseMode) => (mode === 'design' || !d.flexCase ? d.caps : mode);

  const errorSvg = (msg) =>
    OG.logoSvg(
      `<rect width="480" height="300" fill="#AA0000"/>` +
        OG.pixText('DIRECTION FAILED', { x: 32, y: 120, px: 3, fill: '#FFFFFF' }) +
        OG.pixText(String(msg).slice(0, 40), { x: 32, y: 160, px: 1.5, fill: '#FFFF55' })
    );

  function logoSvg(d, word) {
    try {
      return OG.scope(d.logo(word));
    } catch (e) {
      console.error(`#${d.num} logo`, e);
      return errorSvg(e.message);
    }
  }
  function iconSvg(d) {
    try {
      return OG.scope(d.icon());
    } catch (e) {
      console.error(`#${d.num} icon`, e);
      return OG.iconSvg('<rect width="64" height="64" fill="#AA0000"/>');
    }
  }

  // Puts an SVG string inside another at a given box.
  const nest = (svg, x, y, w, h) =>
    svg.replace('<svg ', `<svg x="${x}" y="${y}" width="${w}" height="${h}" preserveAspectRatio="xMidYMid meet" `);

  /* --------------------------------------------------------------- filters */

  const FACETS = [
    { key: 'family', label: 'Family', values: OG.FAMILIES, get: (d) => [d.family] },
    { key: 'sources', label: 'Inspiration', values: OG.SOURCES, get: (d) => d.sources || [] },
    { key: 'paletteTag', label: 'Palette', values: OG.PALETTES, get: (d) => [d.paletteTag] },
    { key: 'risk', label: 'Risk', values: OG.RISKS, get: (d) => [d.risk] },
    { key: 'caps', label: 'Designed in', values: [...OG.CASES, 'Other'], get: (d) => [OG.CASES.includes(d.caps) ? d.caps : 'Other'] },
    { key: 'flex', label: 'Case switch', values: ['Redraws in any case', 'Fixed case'], get: (d) => [d.flexCase ? 'Redraws in any case' : 'Fixed case'] },
  ];
  const active = {};
  FACETS.forEach((f) => (active[f.key] = new Set()));
  let query = '';
  let starOnly = false;
  let shuffleSeed = 1;

  const haystack = new Map(
    ALL.map((d) => [
      d.id,
      [d.num, d.name, d.caps, d.family, ...(d.sources || []), d.paletteTag, d.risk, d.concept, d.type, ...(d.fonts || [])]
        .join(' ')
        .toLowerCase(),
    ])
  );

  // Whether d passes every filter, leaving out the facet named `skip`.
  function passes(d, skip) {
    if (starOnly && !stars.has(d.id)) return false;
    if (query) {
      const hay = haystack.get(d.id) + ' ' + (notes[d.id] || '').toLowerCase();
      if (!query.split(/\s+/).every((t) => hay.includes(t))) return false;
    }
    return FACETS.every((f) => {
      if (f.key === skip) return true;
      const sel = active[f.key];
      return !sel.size || f.get(d).some((v) => sel.has(v));
    });
  }

  const RISK_ORDER = { Safe: 0, Bold: 1, Wild: 2 };
  function visible() {
    const list = ALL.filter((d) => passes(d));
    if (prefs.sort === 'name') list.sort((a, b) => a.name.localeCompare(b.name));
    else if (prefs.sort === 'risk') list.sort((a, b) => RISK_ORDER[a.risk] - RISK_ORDER[b.risk] || a.num - b.num);
    else if (prefs.sort === 'shuffle') {
      const rnd = OG.rng(shuffleSeed);
      const keys = new Map(ALL.map((d) => [d.id, rnd()]));
      list.sort((a, b) => keys.get(a.id) - keys.get(b.id));
    }
    return list;
  }

  function renderFilters() {
    const any = FACETS.some((f) => active[f.key].size) || query || starOnly;
    const nActive = FACETS.reduce((n, f) => n + active[f.key].size, 0);
    // Open at first on wide screens; on a phone the list is long.
    const open = $('#filterbox details') ? $('#filterbox details').open : window.innerWidth > 640;
    let html = `<details${open ? ' open' : ''}><summary>Filters ${nActive ? `<b>${nActive} on</b>` : ''}</summary><div class="groups">`;
    FACETS.forEach((f) => {
      html += `<div class="group"><span class="label">${f.label}</span><div class="chips">`;
      f.values.forEach((v) => {
        const on = active[f.key].has(v);
        const n = ALL.filter((d) => passes(d, f.key) && f.get(d).includes(v)).length;
        const dot = f.key === 'risk' ? `<span class="dot" style="--c:var(--${v.toLowerCase()})"></span>` : '';
        html += `<button class="chip" data-facet="${f.key}" data-value="${esc(v)}" aria-pressed="${on}"${!n && !on ? ' disabled' : ''}>${dot}${esc(v)} <span class="n">${n}</span></button>`;
      });
      html += `</div></div>`;
    });
    html += `</div></details>`;
    if (any) {
      html += `<div class="active-filters">Showing only:`;
      if (starOnly) html += ` <button class="chip" data-clear="star">★ Shortlist</button>`;
      if (query) html += ` <button class="chip" data-clear="query">“${esc(query)}”</button>`;
      FACETS.forEach((f) => active[f.key].forEach((v) => (html += ` <button class="chip" data-facet="${f.key}" data-value="${esc(v)}" aria-pressed="true">${esc(v)}</button>`)));
      html += ` <button class="btn" data-clear="all">Clear all</button></div>`;
    }
    $('#filterbox').innerHTML = html;
  }

  $('#filters').addEventListener('click', (e) => {
    const b = e.target.closest('button');
    if (!b) return;
    if (b.dataset.facet) {
      const set = active[b.dataset.facet];
      const v = b.dataset.value;
      set.has(v) ? set.delete(v) : set.add(v);
    } else if (b.dataset.clear === 'star') {
      setStarOnly(false);
      return;
    } else if (b.dataset.clear === 'query') {
      query = '';
      $('#q').value = '';
    } else if (b.dataset.clear === 'all') {
      FACETS.forEach((f) => active[f.key].clear());
      query = '';
      $('#q').value = '';
      starOnly = false;
      syncStarButton();
    } else return;
    refresh();
  });

  /* ------------------------------------------------------------------ grid */

  // Logos are drawn when their card comes near the screen.
  const observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((en) => {
        if (!en.isIntersecting) return;
        drawCard(en.target);
        observer.unobserve(en.target);
      });
    },
    { rootMargin: '600px 0px' }
  );

  function drawCard(card) {
    const d = byId.get(card.dataset.id);
    const word = wordFor(d);
    const thumb = $('.thumb', card);
    const fixed = prefs.caseMode !== 'design' && !d.flexCase ? `<span class="fixed">${esc(d.caps)} · fixed case</span>` : '';
    thumb.innerHTML = logoSvg(d, word) + fixed;
    thumb.style.background = groundFor(d);
    const icon = $('.icon', card);
    if (!icon.firstChild) {
      icon.innerHTML = iconSvg(d);
      icon.style.background = d.bg;
    }
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) pauseAll(card);
  }

  function pauseAll(root) {
    root.querySelectorAll('svg').forEach((s) => s.pauseAnimations && s.pauseAnimations());
  }

  function cardHtml(d) {
    const starred = stars.has(d.id);
    return `<article class="card" data-id="${d.id}">
      <button class="thumb" data-open="${d.id}" style="background:${groundFor(d)}" aria-label="Open ${d.num}, ${esc(d.name)}"></button>
      <div class="meta">
        <div class="icon" aria-hidden="true"></div>
        <div class="name"><span class="num">${pad(d.num)}</span> ${esc(d.name)}</div>
        <button class="starbtn" data-star="${d.id}" aria-pressed="${starred}" aria-label="Shortlist ${esc(d.name)}">★</button>
        <div class="sub"><span class="risk ${d.risk}">${d.risk}</span><span class="tags">${esc(d.family)} · ${esc((d.sources || []).join(', '))}</span></div>
      </div>
    </article>`;
  }

  function groupsOf(list) {
    const by = (keyFn, order) => {
      const map = new Map(order.map((k) => [k, []]));
      list.forEach((d) => {
        const k = keyFn(d);
        if (!map.has(k)) map.set(k, []);
        map.get(k).push(d);
      });
      return [...map].filter(([, items]) => items.length).map(([title, items]) => ({ title, items }));
    };
    switch (prefs.group) {
      case 'family': return by((d) => d.family, OG.FAMILIES);
      case 'source': return by((d) => (d.sources || [])[0], OG.SOURCES);
      case 'risk': return by((d) => d.risk, OG.RISKS);
      case 'caps': return by((d) => (OG.CASES.includes(d.caps) ? d.caps : 'Other'), [...OG.CASES, 'Other']);
      default: return [{ title: null, items: list }];
    }
  }

  let current = [];
  function renderGrid() {
    current = visible();
    const main = $('#main');
    observer.disconnect();
    if (!current.length) {
      main.innerHTML = `<div class="empty"><b>No directions match.</b>Loosen a filter, or clear them all above.</div>`;
    } else {
      main.innerHTML = groupsOf(current)
        .map((g) => (g.title ? `<div class="section-head"><h2>${esc(g.title)}</h2><span class="n">${g.items.length}</span></div>` : '') + `<div class="grid">${g.items.map(cardHtml).join('')}</div>`)
        .join('');
      main.querySelectorAll('.card').forEach((c) => observer.observe(c));
    }
    $('#count').innerHTML = `<b>${current.length}</b> of ${ALL.length}`;
  }

  function refresh() {
    renderFilters();
    renderGrid();
  }

  $('#main').addEventListener('click', (e) => {
    const s = e.target.closest('[data-star]');
    if (s) return toggleStar(s.dataset.star);
    const o = e.target.closest('[data-open]');
    if (o) openSheet(o.dataset.open);
  });

  /* -------------------------------------------------------------- the bar */

  function renderTitle() {
    const word = prefs.caseMode === 'design' ? 'OgOs/OgOS/OGOS' : prefs.caseMode;
    const rows = OG.textRows(word, { prop: true });
    const w = rows[0].length;
    $('#title').innerHTML = `<svg viewBox="0 0 ${w} 8" shape-rendering="crispEdges" role="img" aria-label="${esc(word)}">${OG.bitmap(rows, { '#': 'currentColor' })}</svg>`;
    $('#title').setAttribute('aria-label', word);
  }

  function renderSummary() {
    const n = (r) => ALL.filter((d) => d.risk === r).length;
    const flex = ALL.filter((d) => d.flexCase).length;
    $('#summary').innerHTML = `
      <div class="total"><b>${ALL.length}</b> directions · ${flex} redraw in any case</div>
      <div class="meter" role="img" aria-label="${OG.RISKS.map((r) => `${n(r)} ${r.toLowerCase()}`).join(', ')}">
        ${OG.RISKS.map((r) => `<i style="width:${(n(r) / ALL.length) * 100}%;background:var(--${r.toLowerCase()})"></i>`).join('')}
      </div>
      <div class="legend">${OG.RISKS.map((r) => `<span style="--c:var(--${r.toLowerCase()})">${n(r)} ${r.toLowerCase()}</span>`).join('')}</div>`;
  }

  function seg(el, options, value, onPick) {
    el.innerHTML = options.map(([v, label]) => `<button data-v="${esc(v)}" aria-pressed="${v === value}">${label}</button>`).join('');
    el.onclick = (e) => {
      const b = e.target.closest('button');
      if (!b) return;
      el.querySelectorAll('button').forEach((x) => x.setAttribute('aria-pressed', x === b));
      onPick(b.dataset.v);
    };
  }

  seg($('#case'), [['design', 'As designed'], ...OG.CASES.map((c) => [c, c])], prefs.caseMode, (v) => {
    prefs.caseMode = v;
    savePrefs();
    renderTitle();
    renderGrid();
    if (openId) openSheet(openId);
  });
  seg(
    $('#ground'),
    Object.entries(GROUNDS).map(([k, g]) => [k, g.color ? `<span class="ground-swatch" style="background:${g.color}"></span> ${g.label}` : g.label]),
    prefs.ground,
    (v) => {
      prefs.ground = v;
      savePrefs();
      renderGrid();
    }
  );

  $('#group').value = prefs.group;
  $('#group').onchange = (e) => {
    prefs.group = e.target.value;
    savePrefs();
    renderGrid();
  };
  $('#sort').value = prefs.sort;
  $('#sort').onchange = (e) => {
    prefs.sort = e.target.value;
    if (prefs.sort === 'shuffle') shuffleSeed = (Date.now() & 0xffff) + 1;
    savePrefs();
    renderGrid();
  };
  const applySize = () => document.documentElement.style.setProperty('--thumb', prefs.size + 'px');
  $('#size').value = prefs.size;
  applySize();
  $('#size').oninput = (e) => {
    prefs.size = +e.target.value;
    applySize();
    savePrefs();
  };

  let qTimer;
  $('#q').addEventListener('input', (e) => {
    clearTimeout(qTimer);
    qTimer = setTimeout(() => {
      query = e.target.value.trim().toLowerCase();
      refresh();
    }, 120);
  });

  /* ------------------------------------------------------------ shortlist */

  function syncStarButton() {
    $('#starCount').textContent = stars.size;
    $('#starOnly').setAttribute('aria-pressed', starOnly);
  }
  function setStarOnly(on) {
    starOnly = on;
    syncStarButton();
    refresh();
  }
  function toggleStar(id) {
    stars.has(id) ? stars.delete(id) : stars.add(id);
    saveStars();
    syncStarButton();
    document.querySelectorAll(`[data-star="${id}"]`).forEach((b) => b.setAttribute('aria-pressed', stars.has(id)));
    if (starOnly && !openId) refresh();
  }
  $('#starOnly').onclick = () => setStarOnly(!starOnly);

  function toast(msg) {
    const t = document.createElement('div');
    t.className = 'toast';
    t.setAttribute('role', 'status');
    t.textContent = msg;
    document.body.appendChild(t);
    setTimeout(() => t.remove(), 2200);
  }

  function copyText(text, what) {
    const fallback = () => {
      const box = document.createElement('div');
      box.className = 'copybox';
      box.innerHTML = `<p>Copying isn't allowed here. Select the text and copy it yourself.</p><textarea readonly></textarea><button class="btn">Close</button>`;
      $('textarea', box).value = text;
      $('button', box).onclick = () => box.remove();
      document.body.appendChild(box);
      $('textarea', box).select();
    };
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(() => toast(`Copied ${what}`), fallback);
    } else fallback();
  }

  $('#copy').onclick = () => {
    const list = ALL.filter((d) => stars.has(d.id));
    if (!list.length) return toast('Star some directions first');
    const lines = [`OgOs shortlist (${list.length})`];
    list.forEach((d) => {
      lines.push(`#${pad(d.num)} ${d.name} [${d.caps}] ${d.family}, ${(d.sources || []).join(', ')}, ${d.risk}`);
      if (notes[d.id]) lines.push(`    note: ${notes[d.id].replace(/\s+/g, ' ')}`);
    });
    copyText(lines.join('\n'), `${list.length} directions`);
  };

  /* --------------------------------------------------------- detail sheet */

  let openId = null;
  let lastFocus = null;
  let neighbors = { prev: null, next: null };

  function chip(facet, v) {
    return `<button class="chip" data-filter="${facet}" data-value="${esc(v)}" title="Filter to ${esc(v)}">${esc(v)}</button>`;
  }

  function crtMockup(d, word) {
    const colors = d.palette.filter((c) => c.toLowerCase() !== d.bg.toLowerCase());
    const segs = Array.from({ length: 12 }, (_, i) => `<i style="background:${i < 8 ? colors[i % colors.length] : 'transparent'}"></i>`).join('');
    return `<figure><div class="crt"><div class="screen" style="background:${d.bg};color:${d.ink}">
        <div class="logo">${logoSvg(d, word)}</div>
        <div class="boot"><div class="progress">${segs}</div><div class="caption">Starting ${esc(word)}…</div></div>
      </div></div><figcaption>Boot screen</figcaption></figure>`;
  }

  function deskMockup(d, word) {
    const ic = iconSvg(d);
    return `<figure><div class="desk" style="background:${d.bg};color:${d.ink}">
        <div class="icons">
          <div class="dicon"><div class="ic">${ic}</div><span>${esc(word)}</span></div>
        </div>
        <div class="about" role="presentation">
          <div class="title"><span>About ${esc(word)}</span><span>×</span></div>
          <div class="body">
            <div class="ic">${iconSvg(d)}</div>
            <div><p><b>${esc(word)}</b> 1.0</p><p>A 32-bit operating system for retro computing.</p><p>Memory: 32,768 KB</p></div>
            <span class="ok">OK</span>
          </div>
        </div>
        <div class="taskbar"><span class="start"><span class="ic">${iconSvg(d)}</span>${esc(word)}</span><span class="clock">12:00</span></div>
      </div><figcaption>Desktop, Start button and About box</figcaption></figure>`;
  }

  function floppyMockup(d, word) {
    // A 3.5" disk, 90 x 94 units: label on top, shutter at the bottom.
    const logo = nest(logoSvg(d, word), 13, 6, 64, 40);
    return `<figure><div class="floppy-wrap"><svg viewBox="0 0 90 94" xmlns="http://www.w3.org/2000/svg">
        <path d="M3 0h81l6 6v85a3 3 0 0 1-3 3H3a3 3 0 0 1-3-3V3a3 3 0 0 1 3-3z" fill="#26272c"/>
        <rect x="10" y="3" width="70" height="48" rx="1.5" fill="#f4f4ef"/>
        <rect x="11" y="4" width="68" height="44" fill="${d.bg}"/>
        ${logo}
        <rect x="24" y="62" width="44" height="32" rx="1" fill="#b9bcc3"/>
        <rect x="27" y="62" width="40" height="32" fill="#a6a9b1"/>
        <rect x="48" y="67" width="10" height="22" rx="1" fill="#26272c"/>
        <rect x="4" y="85" width="5" height="5" fill="#0d0d10"/>
        <rect x="81" y="85" width="5" height="5" fill="#0d0d10"/>
      </svg></div><figcaption>Install disk label</figcaption></figure>`;
  }

  function stickerMockup(d, word) {
    return `<figure><div class="stickers">
        <div class="s1" style="background:${d.bg}">${logoSvg(d, word)}</div>
        <div class="s2" style="background:${d.bg}">${iconSvg(d)}</div>
      </div><figcaption>Stickers</figcaption></figure>`;
  }

  function openSheet(id) {
    const d = byId.get(id);
    if (!d) return;
    if (!openId) lastFocus = document.activeElement;
    openId = id;
    const word = wordFor(d);
    const idx = current.findIndex((x) => x.id === id);
    const prev = idx > 0 ? current[idx - 1] : null;
    const next = idx >= 0 && idx < current.length - 1 ? current[idx + 1] : null;
    neighbors = { prev: prev && prev.id, next: next && next.id };

    const cases = d.flexCase
      ? OG.CASES.map((c) => `<figure class="${c === word ? 'current' : ''}"><div class="tile" style="background:${d.bg}">${logoSvg(d, c)}</div><figcaption>${c}</figcaption></figure>`).join('')
      : '';
    const sizes = [128, 64, 32, 16];
    const marks =
      sizes.map((n) => `<figure><div class="tile" style="width:${n + 16}px;height:${n + 16}px;background:${d.bg}"><div style="width:${n}px;height:${n}px">${iconSvg(d)}</div></div><figcaption>${n} px</figcaption></figure>`).join('') +
      [['#FFFFFF', 'on white'], ['#141414', 'on black']].map(([bg, l]) => `<figure><div class="tile" style="width:80px;height:80px;background:${bg};box-shadow:inset 0 0 0 1px rgb(127 127 127 / .3)"><div style="width:64px;height:64px">${iconSvg(d)}</div></div><figcaption>${l}</figcaption></figure>`).join('');

    const sheet = $('#sheet');
    sheet.innerHTML = `<div class="sheet-inner">
      <div class="sheet-top">
        <span class="num">${pad(d.num)}</span>
        <h2 id="sheetTitle">${esc(d.name)}</h2>
        <button class="starbtn" data-star="${d.id}" aria-pressed="${stars.has(d.id)}" aria-label="Shortlist ${esc(d.name)}">★</button>
        <div class="nav">
          <button class="btn" data-go="${prev ? prev.id : ''}" ${prev ? '' : 'disabled'} aria-label="Previous direction">←</button>
          <button class="btn" data-go="${next ? next.id : ''}" ${next ? '' : 'disabled'} aria-label="Next direction">→</button>
          <button class="btn" data-close aria-label="Close">Close</button>
        </div>
      </div>
      <div class="sheet-body">
        <div class="hero">
          <div class="stage" style="background:${d.bg}">${logoSvg(d, word)}</div>
          <div class="facts">
            <p class="concept">${esc(d.concept)}</p>
            <p class="typenote">${esc(d.type)}</p>
            <dl>
              <dt>Case</dt><dd><span class="mono">${esc(d.caps)}</span>${d.flexCase ? ' · redraws in any case' : ' · fixed'}</dd>
              <dt>Family</dt><dd>${chip('family', d.family)}</dd>
              <dt>Inspiration</dt><dd>${(d.sources || []).map((s) => chip('sources', s)).join('')}</dd>
              <dt>Palette</dt><dd>${chip('paletteTag', d.paletteTag)}</dd>
              <dt>Risk</dt><dd><span class="risk ${d.risk}">${d.risk}</span></dd>
              ${d.fonts && d.fonts.length ? `<dt>Fonts</dt><dd class="mono" style="font-size:12px">${d.fonts.map((f) => esc(f.split(':')[0])).join(', ')}</dd>` : ''}
            </dl>
            <div class="swatches">${[d.bg, ...d.palette.filter((c) => c.toLowerCase() !== d.bg.toLowerCase())].map((c) => `<button class="swatch" data-hex="${c}" title="Copy ${c}"><i style="background:${c}"></i>${c.toUpperCase()}</button>`).join('')}</div>
          </div>
        </div>
        ${d.flexCase ? `<section class="block"><h3>Capitalization</h3><p class="hint">The same design in each case under consideration.</p><div class="cases">${cases}</div></section>` : `<section class="block"><h3>Capitalization</h3><p class="hint">This direction is built around <span class="mono">${esc(d.caps)}</span> and doesn't redraw in other cases.</p></section>`}
        <section class="block"><h3>Mark</h3><p class="hint">The icon alone, down to favicon size.</p><div class="marks">${marks}</div></section>
        ${d.scene ? '' : `<section class="block"><h3>On light and dark</h3><div class="grounds"><figure><div class="tile" style="background:#F5F5F2">${logoSvg(d, word)}</div><figcaption>Light</figcaption></figure><figure><div class="tile" style="background:#131316">${logoSvg(d, word)}</div><figcaption>Dark</figcaption></figure></div></section>`}
        <section class="block"><h3>In use</h3><p class="hint">Rough placements in this direction's colors.</p><div class="mockups">${crtMockup(d, word)}${deskMockup(d, word)}${floppyMockup(d, word)}${stickerMockup(d, word)}</div></section>
        <section class="block notes"><h3><label for="note">Your notes</label></h3><textarea id="note" placeholder="What works, what doesn't. Saved in this browser and included when you copy the shortlist.">${esc(notes[d.id] || '')}</textarea></section>
      </div>
    </div>`;
    sheet.hidden = false;
    sheet.scrollTop = 0;
    document.body.style.overflow = 'hidden';
    if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) pauseAll(sheet);
    try {
      history.replaceState(null, '', '#n' + d.num);
    } catch (e) {
      /* hash not available */
    }
    $('[data-close]', sheet).focus({ preventScroll: true });
  }

  function closeSheet() {
    const sheet = $('#sheet');
    sheet.hidden = true;
    sheet.innerHTML = '';
    document.body.style.overflow = '';
    openId = null;
    try {
      history.replaceState(null, '', location.pathname + location.search);
    } catch (e) {
      /* hash not available */
    }
    if (starOnly) refresh();
    if (lastFocus && lastFocus.focus) lastFocus.focus({ preventScroll: true });
  }

  $('#sheet').addEventListener('click', (e) => {
    if (e.target.id === 'sheet') return closeSheet();
    const b = e.target.closest('button');
    if (!b) return;
    if (b.hasAttribute('data-close')) closeSheet();
    else if (b.dataset.go) openSheet(b.dataset.go);
    else if (b.dataset.star) toggleStar(b.dataset.star);
    else if (b.dataset.hex) copyText(b.dataset.hex, b.dataset.hex);
    else if (b.dataset.filter) {
      FACETS.forEach((f) => active[f.key].clear());
      active[b.dataset.filter].add(b.dataset.value);
      closeSheet();
      refresh();
      $('#filters').scrollIntoView({ behavior: 'smooth', block: 'start' });
    }
  });
  $('#sheet').addEventListener('input', (e) => {
    if (e.target.id !== 'note' || !openId) return;
    const v = e.target.value;
    if (v.trim()) notes[openId] = v;
    else delete notes[openId];
    store.set('notes', notes);
  });

  document.addEventListener('keydown', (e) => {
    const typing = /^(INPUT|TEXTAREA|SELECT)$/.test(e.target.tagName);
    if (openId) {
      if (e.key === 'Escape') return closeSheet();
      if (typing) return;
      if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        const id = e.key === 'ArrowLeft' ? neighbors.prev : neighbors.next;
        if (id) openSheet(id);
      } else if (e.key === 's' || e.key === 'S') toggleStar(openId);
      return;
    }
    if (e.key === '/' && !typing) {
      e.preventDefault();
      $('#q').focus();
    }
  });

  /* ------------------------------------------------------------------ start */

  renderTitle();
  renderSummary();
  syncStarButton();
  refresh();
  const m = /^#n(\d+)$/.exec(location.hash);
  if (m) {
    const d = ALL.find((x) => x.num === +m[1]);
    if (d) openSheet(d.id);
  }
})();
