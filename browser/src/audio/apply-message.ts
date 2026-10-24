/** Apply a control message to a MorphCore (shared by the worklet and the offline renderer). */

import type { MorphCore } from '../dsp/core';
import type { ToWorklet } from './messages';

export function applyMessage(c: MorphCore, msg: ToWorklet): void {
  switch (msg.type) {
    case 'sample': c.setSample(msg.slot, msg.channels, msg.settings); break;
    case 'slotSettings': c.setSlotSettings(msg.slot, msg.settings); break;
    case 'timeline': c.setTimeline(msg.model); break;
    case 'gainLaw': c.setGainLaw(msg.law); break;
    case 'master': c.setMasterGain(msg.gain); break;
    case 'duration': c.setDuration(msg.seconds); break;
    case 'playMode': c.setPlayMode(msg.mode); break;
    case 'direction': c.setDirection(msg.direction); break;
    case 'hold': c.setHold(msg.held); break;
    case 'seek': c.seek(msg.position); break;
    case 'engine': c.setEngine(msg.engine); break;
    case 'granular': c.setGranularParams(msg.params); break;
    case 'spectral': c.setSpectralParams(msg.params); break;
    case 'character': c.setCharacter(msg.value); break;
    case 'drift': c.setDrift(msg.params); break;
    case 'effects': c.setEffects(msg.params); break;
    case 'loopCrossfade': c.loopCrossfadeSeconds = msg.seconds; break;
    case 'transport':
      if (msg.command === 'play') c.play();
      else if (msg.command === 'pause') c.pause();
      else if (msg.command === 'stop') c.stop();
      else c.restart();
      break;
  }
}
