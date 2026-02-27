const WebSocket = require('ws');
const ws = new WebSocket('ws://localhost:20000/devtools/page/F66450AEC52172FB67368635ECDFB03D');
let id = 1;
const send = (m, p) => ws.send(JSON.stringify({id: id++, method: m, params: p || {}}));
const out = [];

ws.on('open', () => {
  // Check wallet balance by calling wallet_status with full params
  send('Runtime.evaluate', {
    expression: `(async () => {
      try {
        // Direct wallet API call to get full status with balance
        const api = window;

        // Check what's available in the BEAM object
        const result = await new Promise((resolve, reject) => {
          const callId = 'balance-check-' + Date.now();
          const request = {
            jsonrpc: '2.0',
            id: callId,
            method: 'wallet_status',
            params: {}
          };

          // Listen for the response
          const origHandler = null;
          const timer = setTimeout(() => resolve('TIMEOUT'), 10000);

          // We need to intercept the response - add a temporary listener
          const checkResult = (json) => {
            try {
              const answer = JSON.parse(json);
              if (answer.id === callId) {
                clearTimeout(timer);
                resolve(JSON.stringify(answer.result, null, 2));
              }
            } catch(e) {}
          };

          // Hook into the existing API result handler
          // For Qt Desktop, the result comes through callWalletApiResult signal
          // We can't easily add another listener, so let's just check what we know
          resolve('Cannot directly call - checking cached state');
        });

        return result;
      } catch(e) {
        return 'Error: ' + e.message;
      }
    })()`,
    returnByValue: true,
    awaitPromise: true
  });

  // Check what the last wallet_status returned (from our app's perspective)
  send('Runtime.evaluate', {
    expression: `(() => {
      // Check if there's any balance display on the page
      const body = document.body.innerText;

      // Look for any BEAM balance info
      const info = [];
      info.push('Pool available: ' + (document.querySelector('[class*=stat]')?.textContent || 'unknown'));

      // Check the full page for any balance indicators
      const allText = body.substring(0, 3000);
      return allText;
    })()`,
    returnByValue: true
  });

  // Try to get the wallet balance via tx_list or get_utxo
  send('Runtime.evaluate', {
    expression: `(async () => {
      // Try calling get_utxo to see available UTXOs
      return new Promise((resolve) => {
        const callId = 'utxo-check-' + Date.now();
        const origLog = console.log;

        // Temporarily intercept console.log to capture the API response
        let captured = '';
        const interceptor = function() {
          const msg = Array.from(arguments).join(' ');
          if (msg.includes(callId)) {
            captured = msg;
          }
          origLog.apply(console, arguments);
        };
        console.log = interceptor;

        const request = {
          jsonrpc: '2.0',
          id: callId,
          method: 'get_utxo',
          params: { count: 10, skip: 0 }
        };

        try {
          // Access the BEAM API directly
          const channel = document.querySelector('script[src*="qtwebchannel"]');
          // The API is stored somewhere in the app's closure
          // Let's try to find it

          // Actually, we can use the app's own callWalletApi
          // But it's in a module scope...

          // Let's just check if qt.webChannelTransport exists
          if (typeof qt !== 'undefined' && qt.webChannelTransport) {
            resolve('Qt transport available - wallet is connected');
          } else {
            resolve('No Qt transport found');
          }
        } catch(e) {
          resolve('Error: ' + e.message);
        }

        setTimeout(() => {
          console.log = origLog;
          resolve(captured || 'No UTXO response captured');
        }, 3000);
      });
    })()`,
    returnByValue: true,
    awaitPromise: true
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

setTimeout(() => { console.log(out.join('\n---\n')); ws.close(); process.exit(); }, 6000);
