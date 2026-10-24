/** The complete list of control messages that reproduces a project state in a MorphCore. */

import type { ToWorklet } from '../audio/messages';
import { dbToGain } from '../dsp/curves';
import { MAX_SLOTS, emptySlot, slotSettings, timelineModel, type ProjectState } from './project';

export function stateMessages(s: ProjectState): ToWorklet[] {
  const out: ToWorklet[] = [];
  s.slots.forEach((slot, i) => out.push({ type: 'sample', slot: i, channels: slot.audio?.channels ?? null, settings: slotSettings(slot) }));
  for (let i = s.slots.length; i < MAX_SLOTS; i++) out.push({ type: 'sample', slot: i, channels: null, settings: slotSettings(emptySlot()) });
  out.push(
    { type: 'duration', seconds: s.durationSec },
    { type: 'playMode', mode: s.playMode },
    { type: 'direction', direction: s.direction },
    { type: 'gainLaw', law: s.gainLaw },
    { type: 'master', gain: dbToGain(s.masterDb) },
    { type: 'loopCrossfade', seconds: s.loopCrossfadeMs / 1000 },
    { type: 'granular', params: s.granular },
    { type: 'spectral', params: s.spectral },
    { type: 'character', value: s.character },
    { type: 'drift', params: s.drift },
    { type: 'effects', params: s.effects },
    { type: 'engine', engine: s.engine },
    { type: 'timeline', model: timelineModel(s) },
  );
  return out;
}
