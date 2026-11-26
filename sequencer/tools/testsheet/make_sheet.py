#!/usr/bin/env python3
"""Build the combined hardware test sheet: Markdown + phone page.

  python3 tools/testsheet/make_sheet.py OUT.md OUT.html

The page saves ticks and comments on the phone straight away and, when it is
opened as a claude.ai artifact with the `artifact` capability, republishes
itself with the results inside, so they can be read back later.
"""
import json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tests import SECTIONS, BUILD, BUILD_URL, BEFORE

def md(out):
    n = sum(len(s[3]) for s in SECTIONS)
    L = ["# Genvolve: one test sheet for everything since build #133", "",
         f"{n} tests in {len(SECTIONS)} parts. Everything built after build #133 "
         "(PR 29 to PR 43) that has not been tested on the hardware yet, with the "
         "repeats taken out. All on the handheld and the FM1, no SSH.", "",
         f"**Build:** {BUILD}: {BUILD_URL} (artifact **genvolve-knulli-aarch64**).", "",
         f"**Before you start:** {BEFORE}", "",
         "The phone version (tap PASS / FAIL / SKIP, type a comment, copy the "
         "results) is the artifact page linked in the project thread.", "",
         "| Part | What | Tests |", "|---|---|---|"]
    for sid, title, note, tests in SECTIONS:
        L.append(f"| {sid} | {title} | {len(tests)} |")
    for sid, title, note, tests in SECTIONS:
        L += ["", "---", "", f"## {sid}. {title}", ""]
        if note:
            L += [note, ""]
        for tid, tt, steps, exp in tests:
            L.append(f"#### {tid}. {tt}")
            L += [f"{i}. {s}" for i, s in enumerate(steps, 1)]
            L += ["", f"**Expected:** {exp}", "",
                  "[ ] PASS  [ ] FAIL  [ ] SKIP   Note: ______________________________", ""]
    open(out, "w").write("\n".join(L).rstrip() + "\n")

CSS = r"""
/* VOICE: a workshop test log, calm and quick to scan on a phone at night.
   IDEA: a printed inspection card clipped to a board: numbered checks, a stamp box each.
   PICTURE: the sheet itself; the progress strip is a row of stamps, one per test.
   COLOUR and TYPE: graphite ink on warm white, pass green, fail red, skip grey; Archivo + Inter.
   LAYOUT: one column, sticky progress bar, part chips, cards with big tap targets. */
:root{color-scheme:light dark;--bg:light-dark(#f6f4ef,#15171a);--card:light-dark(#fff,#1e2125);--ink:light-dark(#1b1d20,#eceae4);--mute:light-dark(#63666b,#a3a6ab);--line:light-dark(#dcd8cf,#33373d);--ok:light-dark(#137a43,#4cc282);--okbg:light-dark(#e2f4e9,#163626);--bad:light-dark(#c0292b,#ff6b67);--badbg:light-dark(#fbe4e3,#3e1d1d);--skip:light-dark(#77797d,#8e9196);--skbg:light-dark(#ecebe7,#2a2d31);--acc:light-dark(#1b1d20,#eceae4)}
:root[data-theme=light]{color-scheme:light}:root[data-theme=dark]{color-scheme:dark}
*{box-sizing:border-box;min-width:0}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 Inter,system-ui,sans-serif;overflow-x:clip}
h1,h2,h3{font-family:Archivo,Inter,sans-serif;margin:0;text-wrap:balance}
h1{font-size:clamp(26px,6vw,40px);line-height:1.08}
h2{font-size:22px;line-height:1.15;padding:28px 0 4px}
h3{font-size:17px;line-height:1.25}
a{color:inherit}
.wrap{max-width:760px;margin:0 auto;padding:0 max(4vw,14px)}
header.top{padding:22px 0 10px}
.lede{color:var(--mute);margin:8px 0 0}
.before{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;margin:14px 0 0;font-size:15px}
.bar{position:sticky;top:0;z-index:5;background:var(--bg);border-bottom:1px solid var(--line);padding:8px 0}
.stamps{display:flex;gap:2px;height:10px}
.stamps i{flex:1;border-radius:2px;background:var(--line)}
.stamps i.P{background:var(--ok)}.stamps i.F{background:var(--bad)}.stamps i.S{background:var(--skip)}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-top:8px}
.count{font-weight:600;font-size:15px;margin-right:auto}
.count b{color:var(--bad)}
.chip,.btn{font:inherit;font-size:15px;border:1px solid var(--line);background:var(--card);color:var(--ink);border-radius:999px;padding:6px 12px;min-height:38px;cursor:pointer}
.chip[aria-pressed=true]{background:var(--ink);color:var(--bg);border-color:var(--ink)}
.btn.main{background:var(--ink);color:var(--bg);border-color:var(--ink);font-weight:600}
.parts{display:flex;gap:6px;overflow-x:auto;padding:8px 0 2px;scrollbar-width:none}
.parts a{flex:none;text-decoration:none;font-size:14px;border:1px solid var(--line);border-radius:8px;padding:6px 10px;background:var(--card)}
.parts a.done{border-color:var(--ok)}.parts a.bad{border-color:var(--bad);color:var(--bad)}
.snote{color:var(--mute);font-size:15px;margin:2px 0 8px}
.card{background:var(--card);border:1px solid var(--line);border-left:5px solid var(--line);border-radius:10px;padding:14px;margin:10px 0}
.card.P{border-left-color:var(--ok)}.card.F{border-left-color:var(--bad)}.card.S{border-left-color:var(--skip)}
.card h3 span{color:var(--mute);font-weight:600;margin-right:6px}
.card ol{margin:8px 0;padding-left:22px}.card li{margin:3px 0}
.exp{background:light-dark(#f3f1ec,#24272c);border-radius:8px;padding:8px 10px;font-size:15px}
.exp em{font-style:normal;font-weight:700;margin-right:4px}
.pick{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin-top:10px}
.pick button{font:inherit;font-weight:700;min-height:48px;border-radius:9px;border:2px solid var(--line);background:transparent;color:var(--ink);cursor:pointer}
.pick button.P[aria-pressed=true]{background:var(--okbg);border-color:var(--ok);color:var(--ok)}
.pick button.F[aria-pressed=true]{background:var(--badbg);border-color:var(--bad);color:var(--bad)}
.pick button.S[aria-pressed=true]{background:var(--skbg);border-color:var(--skip);color:var(--skip)}
textarea{width:100%;margin-top:8px;font:16px/1.4 Inter,system-ui,sans-serif;color:var(--ink);background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:9px 10px;min-height:46px;resize:vertical}
button:focus-visible,textarea:focus-visible,a:focus-visible{outline:3px solid light-dark(#2a6fdb,#7fb0ff);outline-offset:2px}
.saved{font-size:13px;color:var(--mute)}
.out{margin:16px 0 40px}.out textarea{min-height:180px;font-size:14px}
.hide{display:none}
footer{color:var(--mute);font-size:13px;padding:10px 0 40px}
@media (prefers-reduced-motion:no-preference){.card{transition:border-color .2s}}
"""

APP = r"""{
const DATA=JSON.parse(document.getElementById('data').textContent);
const EMB=JSON.parse(document.getElementById('state').textContent);
const CSS=document.getElementById('css').textContent;
const APP=document.getElementById('app').textContent;
const KEY='genvolve-sheet-'+DATA.key;
let S=EMB, dirty=false, art=null, timer=null, filter='all';
try{const L=JSON.parse(localStorage.getItem(KEY)||'null');if(L&&L.ts>(EMB.ts||0)){S=L;dirty=true}}catch(e){}
if(!S.r)S.r={};
const esc=t=>String(t).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt=t=>esc(t).replace(/\*\*(.+?)\*\*/g,'<b>$1</b>');
const all=DATA.sections.flatMap(s=>s.tests);
const root=document.getElementById('root');
function stat(){let p=0,f=0,k=0;for(const t of all){const v=(S.r[t.id]||{}).v;if(v=='P')p++;else if(v=='F')f++;else if(v=='S')k++}return{p,f,k,d:p+f+k}}
function card(t){const r=S.r[t.id]||{};return `<article class="card ${r.v||''}" id="t${esc(t.id).replace(/\./g,'-')}" data-id="${esc(t.id)}">
<h3><span>${esc(t.id)}</span>${fmt(t.title)}</h3><ol>${t.steps.map(s=>`<li>${fmt(s)}</li>`).join('')}</ol>
<div class="exp"><em>Expected:</em>${fmt(t.exp)}</div>
<div class="pick">${[['P','Pass'],['F','Fail'],['S','Skip']].map(([k,l])=>`<button type="button" class="${k}" data-v="${k}" aria-pressed="${r.v==k}">${l}</button>`).join('')}</div>
<textarea data-c placeholder="Comment: what you saw (optional)" aria-label="Comment for test ${esc(t.id)}">${esc(r.c||'')}</textarea></article>`}
function render(){
 const st=stat();
 root.innerHTML=`<div class="wrap"><header class="top"><h1>Genvolve test sheet</h1>
<p class="lede">${all.length} tests for everything built since build #133, repeats taken out. Tap Pass, Fail or Skip and add a comment if something looks off. It saves as you go.</p>
<div class="before"><b>Build:</b> <a href="${esc(DATA.url)}">${esc(DATA.build)}</a>, artifact <b>genvolve-knulli-aarch64</b>.<br><b>Before you start:</b> ${fmt(DATA.before)}</div></header></div>
<div class="bar"><div class="wrap"><div class="stamps" aria-hidden="true">${all.map(t=>`<i class="${(S.r[t.id]||{}).v||''}"></i>`).join('')}</div>
<div class="row"><span class="count">${st.d} of ${all.length} done${st.f?` · <b>${st.f} failed</b>`:''}</span>
<button class="chip" data-f="all" aria-pressed="${filter=='all'}">All</button><button class="chip" data-f="todo" aria-pressed="${filter=='todo'}">To do</button><button class="chip" data-f="fail" aria-pressed="${filter=='fail'}">Fails</button>
<button class="btn main" id="copy">Copy results</button></div>
<nav class="parts">${DATA.sections.map(s=>{const v=s.tests.map(t=>(S.r[t.id]||{}).v);const cls=v.includes('F')?'bad':v.every(x=>x)?'done':'';return `<a class="${cls}" href="#p${esc(s.id)}">${esc(s.id)}. ${esc(s.title)}</a>`}).join('')}</nav>
<div class="saved" id="saved"></div></div></div>
<div class="wrap" id="list">${DATA.sections.map(s=>`<section id="p${esc(s.id)}"><h2>${esc(s.id)}. ${esc(s.title)}</h2>${s.note?`<p class="snote">${fmt(s.note)}</p>`:''}${s.tests.map(card).join('')}</section>`).join('')}
<div class="out"><button class="btn main" id="copy2">Copy results</button> <span class="saved">Paste them in the project thread.</span><textarea id="outtxt" class="hide" readonly aria-label="Results text"></textarea></div>
<footer>Ticks are kept on this phone at once and saved into this page when you stop for a few seconds.</footer></div>`;
 applyFilter();savedMsg();
}
function applyFilter(){for(const c of root.querySelectorAll('.card')){const v=(S.r[c.dataset.id]||{}).v;c.classList.toggle('hide',filter=='todo'?!!v:filter=='fail'?v!='F':false)}
 for(const s of root.querySelectorAll('section'))s.classList.toggle('hide',!s.querySelector('.card:not(.hide)'))}
let msg='';function savedMsg(){const e=document.getElementById('saved');if(e)e.textContent=msg}
function refreshTop(){const st=stat();root.querySelector('.count').innerHTML=`${st.d} of ${all.length} done${st.f?` · <b>${st.f} failed</b>`:''}`;
 root.querySelectorAll('.stamps i').forEach((i,n)=>i.className=(S.r[all[n].id]||{}).v||'');
 root.querySelectorAll('.parts a').forEach((a,n)=>{const v=DATA.sections[n].tests.map(t=>(S.r[t.id]||{}).v);a.className=v.includes('F')?'bad':v.every(x=>x)?'done':''})}
function keep(){S.ts=Date.now();dirty=true;try{localStorage.setItem(KEY,JSON.stringify(S));msg='Saved on this phone.'}catch(e){msg=''}savedMsg();clearTimeout(timer);timer=setTimeout(push,3500)}
function doc(){const js=JSON.stringify(S).replace(/</g,'\\u003c');const dj=JSON.stringify(DATA).replace(/</g,'\\u003c');
 return '<!DOCTYPE html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Genvolve test sheet</title><link rel="preconnect" href="https://fonts.googleapis.com"><link href="https://fonts.googleapis.com/css2?family=Archivo:wght@700&family=Inter:wght@400;600;700&display=swap" rel="stylesheet"><style id="css">'+CSS+'</style></head><body><main id="root"></main><script id="data" type="application/json">'+dj+'<\/script><script id="state" type="application/json">'+js+'<\/script><script id="app">'+APP+'<\/script></body></html>'}
async function push(){if(!dirty||!art)return;if(document.activeElement&&document.activeElement.matches('textarea[data-c]')){clearTimeout(timer);timer=setTimeout(push,3500);return}
 try{sessionStorage.setItem(KEY+'-y',String(scrollY))}catch(e){}
 try{dirty=false;await art.publish(doc());msg='Saved on this phone and in the page.'}catch(e){msg=(e&&/conflict/.test(String(e.code||e.message||e)))?'Saved on this phone.':'Saved on this phone only.'}savedMsg()}
function text(){const st=stat();let o=[`Genvolve test results (${DATA.build}): ${st.p} pass, ${st.f} fail, ${st.k} skip, ${all.length-st.d} not done`];
 for(const s of DATA.sections){o.push('',`${s.id}. ${s.title}`);for(const t of s.tests){const r=S.r[t.id]||{};const v={P:'PASS',F:'FAIL',S:'SKIP'}[r.v]||'not done';o.push(`${t.id} ${t.title}: ${v}${r.c?' - '+r.c.trim():''}`)}}return o.join('\n')}
async function copy(){const t=text();const ta=document.getElementById('outtxt');ta.value=t;try{await navigator.clipboard.writeText(t);msg='Results copied. Paste them in the thread.';ta.classList.add('hide')}catch(e){ta.classList.remove('hide');ta.focus();ta.select();msg='Copy was blocked: select the text below and copy it.'}savedMsg()}
render();
try{const y=+sessionStorage.getItem(KEY+'-y');if(y){sessionStorage.removeItem(KEY+'-y');requestAnimationFrame(()=>scrollTo(0,y))}}catch(e){}
root.addEventListener('click',e=>{const b=e.target.closest('button');if(!b)return;
 if(b.dataset.v){const c=b.closest('.card'),id=c.dataset.id,r=S.r[id]||(S.r[id]={});r.v=r.v==b.dataset.v?'':b.dataset.v;
  c.className='card '+(r.v||'');c.querySelectorAll('.pick button').forEach(x=>x.setAttribute('aria-pressed',x.dataset.v==r.v));refreshTop();keep();return}
 if(b.dataset.f){filter=b.dataset.f;root.querySelectorAll('.chip').forEach(x=>x.setAttribute('aria-pressed',x==b));applyFilter();return}
 if(b.id=='copy'||b.id=='copy2')copy()});
root.addEventListener('input',e=>{if(!e.target.matches('textarea[data-c]'))return;const id=e.target.closest('.card').dataset.id;(S.r[id]||(S.r[id]={})).c=e.target.value;keep()});
root.addEventListener('focusout',e=>{if(e.target.matches('textarea[data-c]')&&dirty){clearTimeout(timer);timer=setTimeout(push,1500)}});
(async()=>{try{art=window.claude?await claude.use('artifact'):null}catch(e){art=null}})();
}"""

def html(out):
    data = {"key": "r240", "build": BUILD, "url": BUILD_URL, "before": BEFORE,
            "sections": [{"id": s, "title": t, "note": n,
                          "tests": [{"id": i, "title": tt, "steps": st, "exp": e}
                                    for i, tt, st, e in tests]}
                         for s, t, n, tests in SECTIONS]}
    dj = json.dumps(data, ensure_ascii=False).replace("<", "\\u003c")
    st = json.dumps({"r": {}, "ts": 0})
    doc = ('<!DOCTYPE html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
           '<title>Genvolve test sheet</title><link rel="preconnect" href="https://fonts.googleapis.com">'
           '<link href="https://fonts.googleapis.com/css2?family=Archivo:wght@700&family=Inter:wght@400;600;700&display=swap" rel="stylesheet">'
           f'<style id="css">{CSS}</style></head><body><main id="root"></main>'
           f'<script id="data" type="application/json">{dj}</script>'
           f'<script id="state" type="application/json">{st}</script>'
           f'<script id="app">{APP}</script></body></html>')
    open(out, "w").write(doc)

if __name__ == "__main__":
    md(sys.argv[1]); html(sys.argv[2])
