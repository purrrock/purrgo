// ============================================================
// Simple logger with levels and colored output
// ============================================================

export enum LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
  SILENT = 4,
}

const LEVEL_LABELS: Record<LogLevel, string> = {
  [LogLevel.DEBUG]: '\x1b[90mDEBUG\x1b[0m',
  [LogLevel.INFO]: '\x1b[36mINFO\x1b[0m ',
  [LogLevel.WARN]: '\x1b[33mWARN\x1b[0m ',
  [LogLevel.ERROR]: '\x1b[31mERROR\x1b[0m',
  [LogLevel.SILENT]: '',
};

const SPINNER_FRAMES = ['⠋', '⠙', '⠹', '⠸', '⠼', '⠴', '⠦', '⠧', '⠇', '⠏'];

let currentLevel: LogLevel = LogLevel.INFO;
let _spinnerTimer: ReturnType<typeof setInterval> | null = null;
let _spinnerFrame = 0;

export function setLogLevel(level: LogLevel): void {
  currentLevel = level;
}

function log(level: LogLevel, ...args: unknown[]): void {
  if (level < currentLevel) return;
  // Clear any active spinner line before printing a real log line
  if (_spinnerTimer) process.stdout.write('\r\x1b[K');
  const timestamp = new Date().toLocaleTimeString().slice(0, 8);
  const label = LEVEL_LABELS[level];
  console.log(`\x1b[90m${timestamp}\x1b[0m ${label}`, ...args);
  // Redraw spinner after the log line (so it lands on a fresh line)
  // The next tick of the interval will repaint it naturally.
}

export const logger = {
  debug: (...args: unknown[]) => log(LogLevel.DEBUG, ...args),
  info: (...args: unknown[]) => log(LogLevel.INFO, ...args),
  warn: (...args: unknown[]) => log(LogLevel.WARN, ...args),
  error: (...args: unknown[]) => log(LogLevel.ERROR, ...args),

  /** Log a success message in green */
  success: (...args: unknown[]) => {
    if (LogLevel.INFO < currentLevel) return;
    if (_spinnerTimer) process.stdout.write('\r\x1b[K');
    const timestamp = new Date().toLocaleTimeString().slice(0, 8);
    console.log(`\x1b[90m${timestamp}\x1b[0m \x1b[32m OK \x1b[0m `, ...args);
  },

  /** Log a file processing message */
  file: (action: string, filePath: string) => {
    if (LogLevel.INFO < currentLevel) return;
    if (_spinnerTimer) process.stdout.write('\r\x1b[K');
    const timestamp = new Date().toLocaleTimeString().slice(0, 8);
    console.log(
      `\x1b[90m${timestamp}\x1b[0m \x1b[36mINFO\x1b[0m  ${action} \x1b[90m${filePath}\x1b[0m`,
    );
  },

  /**
   * Start a spinner with a dynamic message callback.
   * The callback is called on every tick and should return the current message.
   * Call `stopProgress()` to stop the spinner and clear the line.
   */
  startProgress: (getMessage: () => string): void => {
    if (LogLevel.INFO >= currentLevel && _spinnerTimer === null) {
      _spinnerFrame = 0;
      _spinnerTimer = setInterval(() => {
        const frame = SPINNER_FRAMES[_spinnerFrame % SPINNER_FRAMES.length];
        _spinnerFrame++;
        const msg = getMessage();
        process.stdout.write(`\r  \x1b[36m${frame}\x1b[0m ${msg}`);
      }, 100);
    }
  },

  /** Stop the spinner and clear the progress line. */
  stopProgress: (): void => {
    if (_spinnerTimer !== null) {
      clearInterval(_spinnerTimer);
      _spinnerTimer = null;
      process.stdout.write('\r\x1b[K');
    }
  },
};
