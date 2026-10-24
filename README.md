# SlowMorph

An experimental sample-morphing instrument for long-form ambient, drone and
electroacoustic music. Load a few short samples, set a morph time from
5 seconds to 2 hours, and SlowMorph transforms continuously from one sample
into the next, with crossfade, granular and spectral morph engines.

| Folder | What it is | State |
|---|---|---|
| [`browser/`](browser/) | The instrument as a local web app for Chrome or Edge on macOS (TypeScript, Web Audio, AudioWorklet). No cloud services. | Working and tested |
| [`firmware/`](firmware/) | A future port to the M-VAVE FM1 pocket synth. | Feasibility study only |

## Play it

Open **https://r7819smith-oss.github.io/slowmorph/** in Chrome or Edge.
Nothing to install; your samples stay on your computer.

## Run it locally

Requires Node.js 18 or newer.

```sh
cd browser
npm install
npm run dev        # then open http://localhost:5173 in Chrome or Edge
```

See [browser/README.md](browser/README.md) for how to play it and
[browser/ARCHITECTURE.md](browser/ARCHITECTURE.md) for how the engines work
and their limitations.

## Licence

GPL-3.0 only. See [LICENSE](LICENSE). The licence matches the FM1 open
firmware kit's app code, so code can move between the two folders.
