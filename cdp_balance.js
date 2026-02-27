const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:20000/devtools/page/F66450AEC52172FB67368635ECDFB03D');
let id = 1;
const send = (m, p) => ws.send(JSON.stringify({id: id++, method: m, params: p || {}}));
const out = [];

ws.on('open', () => {
  // Enable console to capture API responses
  send('Runtime.enable');
  send('Console.enable');

  // Call wallet_status with assets flag to get full balance info
  send('Runtime.evaluate', {
    expression: `(async () => {
      const results = {};

      // 1. Try wallet_status
      try {
        const status = await new Promise((resolve, reject) => {
          const callId = 'dbg-status-' + Date.now();
          const timer = setTimeout(() => reject('timeout'), 10000);

          // We need to hook into the existing response handler
          // The app stores calls in a Calls object, but it's in module scope
          // Instead, let's make our own direct Qt WebChannel call

          if (typeof qt !== 'undefined' && qt.webChannelTransport) {
            // Get the BEAM API from QWebChannel
            new QWebChannel(qt.webChannelTransport, (channel) => {
              const api = channel.objects.BEAM.api;

              // Hook result handler
              const handler = (json) => {
                try {
                  const answer = JSON.parse(json);
                  if (answer.id === callId) {
                    clearTimeout(timer);
                    resolve(answer);
                  }
                } catch(e) {}
              };

              api.callWalletApiResult.connect(handler);

              // Make wallet_status call
              api.callWalletApi(JSON.stringify({
                jsonrpc: '2.0',
                id: callId,
                method: 'wallet_status',
                params: { nz_totals: true }
              }));
            });
          } else {
            reject('No Qt transport');
          }
        });
        results.wallet_status = status;
      } catch(e) {
        results.wallet_status_error = String(e);
      }

      return JSON.stringify(results, null, 2);
    })()`,
    returnByValue: true,
    awaitPromise: true
  });
});

ws.on('message', (data) => {
  const m = JSON.parse(data.toString());
  if (m.id && m.result && m.result.result && m.result.result.value != null) {
    out.push(m.result.result.value);
  }
  if (m.id && m.result && m.result.exceptionDetails) {
    const ex = m.result.exceptionDetails;
    out.push('ERROR: ' + (ex.exception ? ex.exception.description : ex.text));
  }
});

setTimeout(() => { console.log(out.join('\n')); ws.close(); process.exit(); }, 12000);
