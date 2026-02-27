const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:20000/devtools/page/F66450AEC52172FB67368635ECDFB03D');
let id = 1;
const send = (m, p) => ws.send(JSON.stringify({id: id++, method: m, params: p || {}}));
const out = [];

ws.on('open', () => {
  // Get page text content
  send('Runtime.evaluate', {
    expression: 'document.body.innerText.substring(0, 2000)',
    returnByValue: true
  });
  // Check for error elements
  send('Runtime.evaluate', {
    expression: '(() => { const errs = document.querySelectorAll("[class*=error],[class*=Error],[role=alert]"); return errs.length + " error elements: " + Array.from(errs).map(e => e.textContent.substring(0,100)).join(" | "); })()',
    returnByValue: true
  });
  // Check toast notifications
  send('Runtime.evaluate', {
    expression: '(() => { const toasts = document.querySelectorAll("[class*=toast],[class*=Toast]"); return toasts.length + " toasts: " + Array.from(toasts).map(e => e.textContent.substring(0,100)).join(" | "); })()',
    returnByValue: true
  });
});

ws.on('message', (data) => {
  const m = JSON.parse(data.toString());
  if (m.id && m.result && m.result.result && m.result.result.value != null) {
    out.push('[EVAL id=' + m.id + '] ' + m.result.result.value);
  }
  if (m.id && m.result && m.result.exceptionDetails) {
    const ex = m.result.exceptionDetails;
    out.push('[ERROR] ' + (ex.exception ? ex.exception.description : ex.text));
  }
});

setTimeout(() => { console.log(out.join('\n---\n')); ws.close(); process.exit(); }, 2000);
