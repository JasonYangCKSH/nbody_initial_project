import type { LogEntry, StepMetrics } from '../types';

export class Instrumentation {
  private readonly steps: StepMetrics[] = [];
  private readonly events: LogEntry[] = [];
  recordStep(metrics: StepMetrics): void { this.steps.push(metrics); if (this.steps.length > 240) this.steps.shift(); }
  recordEvent(event: LogEntry): void { this.events.unshift(event); if (this.events.length > 30) this.events.pop(); }
  getHistory(): StepMetrics[] { return this.steps; }
  getEventLog(): LogEntry[] { return this.events; }
  exportJSON(): string { return JSON.stringify({ steps: this.steps, events: this.events }, null, 2); }
}
