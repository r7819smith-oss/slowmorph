/** Tiny DOM helpers: no framework, just typed element creation. */

type Props = Record<string, unknown> & { class?: string; style?: string };
type Child = Node | string | number | null | undefined | false;

export function h<K extends keyof HTMLElementTagNameMap>(tag: K, props: Props = {}, ...children: Child[]): HTMLElementTagNameMap[K] {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(props)) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'class') el.className = String(v);
    else if (k === 'style') el.setAttribute('style', String(v));
    else if (k.startsWith('on') && typeof v === 'function') el.addEventListener(k.slice(2).toLowerCase(), v as EventListener);
    else if (k in el && typeof v !== 'string') (el as unknown as Record<string, unknown>)[k] = v;
    else el.setAttribute(k, v === true ? '' : String(v));
  }
  append(el, children);
  return el;
}

export function append(el: Element, children: Child[]): void {
  for (const c of children) {
    if (c === null || c === undefined || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
}

export function clear(el: Element): void {
  while (el.firstChild) el.removeChild(el.firstChild);
}

/** Size a canvas for the device pixel ratio; returns a context scaled to CSS pixels. */
export function fitCanvas(canvas: HTMLCanvasElement): { ctx: CanvasRenderingContext2D; w: number; h: number } {
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const w = Math.max(1, Math.round(rect.width));
  const hgt = Math.max(1, Math.round(rect.height));
  if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(hgt * dpr)) {
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(hgt * dpr);
  }
  const ctx = canvas.getContext('2d')!;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { ctx, w, h: hgt };
}

export const SLOT_COLORS = ['#f2a65a', '#4fd1c5', '#a78bfa', '#f472b6', '#a3e635', '#60a5fa', '#fb7185', '#facc15'];

export function slotColor(index: number): string {
  return SLOT_COLORS[index % SLOT_COLORS.length];
}

/** A labelled range slider with a live value readout. */
export function slider(opts: {
  label: string;
  min: number;
  max: number;
  step: number;
  value: number;
  format?: (v: number) => string;
  onInput: (v: number) => void;
  title?: string;
  disabled?: boolean;
}): { root: HTMLElement; input: HTMLInputElement; set: (v: number) => void } {
  const fmt = opts.format ?? ((v: number) => String(v));
  const out = h('output', {}, fmt(opts.value));
  const input = h('input', {
    type: 'range',
    min: String(opts.min),
    max: String(opts.max),
    step: String(opts.step),
    value: String(opts.value),
    disabled: opts.disabled,
    'aria-label': opts.label,
  });
  input.addEventListener('input', () => {
    const v = parseFloat(input.value);
    out.textContent = fmt(v);
    opts.onInput(v);
  });
  input.addEventListener('dblclick', () => {
    // double-click resets to the initial value
    input.value = String(opts.value);
    input.dispatchEvent(new Event('input'));
  });
  const root = h('label', { class: 'slider', title: opts.title }, h('span', { class: 'slider-label' }, opts.label), input, out);
  return {
    root,
    input,
    set: (v: number) => {
      if (document.activeElement === input) return;
      input.value = String(v);
      out.textContent = fmt(v);
    },
  };
}

/** A segmented control (radio-like button group). */
export function segmented<T extends string>(
  options: { value: T; label: string; title?: string; disabled?: boolean }[],
  value: T,
  onChange: (v: T) => void,
  ariaLabel: string,
): { root: HTMLElement; set: (v: T) => void } {
  const buttons = options.map((o) =>
    h('button', { type: 'button', title: o.title, disabled: o.disabled, onclick: () => onChange(o.value), 'data-value': o.value }, o.label),
  );
  const root = h('div', { class: 'segmented', role: 'group', 'aria-label': ariaLabel }, ...buttons);
  const set = (v: T) => buttons.forEach((b) => b.classList.toggle('on', b.dataset.value === v));
  set(value);
  return { root, set };
}
