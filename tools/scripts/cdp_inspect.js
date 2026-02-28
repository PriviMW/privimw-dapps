const WebSocket = require('ws');
const http = require('http');

http.get('http://localhost:20000/json', (res) => {
  let data = '';
  res.on('data', chunk => data += chunk);
  res.on('end', () => {
    const targets = JSON.parse(data);
    const beambet = targets.find(t => t.title === 'BeamBet' || t.url.includes('3000'));
    if (!beambet) { console.log('No target'); process.exit(1); }
    connect(beambet.webSocketDebuggerUrl);
  });
});

function connect(wsUrl) {
  const ws = new WebSocket(wsUrl);
  let id = 1;
  const send = (m, p) => ws.send(JSON.stringify({id: id++, method: m, params: p || {}}));

  ws.on('open', () => {
    send('Runtime.enable');

    // Query the wallet for recent transactions to see failure details
    send('Runtime.evaluate', {
      expression: `(async () => {
        // Make a direct wallet API call to get tx_list
        return new Promise((resolve) => {
          const callId = 'txlist-' + Date.now();

          new QWebChannel(qt.webChannelTransport, (channel) => {
            const api = channel.objects.BEAM.api;

            const handler = (json) => {
              try {
                const answer = JSON.parse(json);
                if (answer.id === callId) {
                  resolve(JSON.stringify(answer, null, 2));
                }
              } catch(e) {}
            };
            api.callWalletApiResult.connect(handler);

            api.callWalletApi(JSON.stringify({
              jsonrpc: '2.0',
              id: callId,
              method: 'tx_list',
              params: { count: 10, skip: 0 }
            }));
          });

          setTimeout(() => resolve('TIMEOUT'), 10000);
        });
      })()`,
      returnByValue: true,
      awaitPromise: true
    });
  });

  ws.on('message', (data) => {
    const m = JSON.parse(data.toString());
    if (m.id && m.result && m.result.result && m.result.result.value != null) {
      console.log(m.result.result.value);
    }
    if (m.id && m.result && m.result.exceptionDetails) {
      console.log('ERROR:', m.result.exceptionDetails.exception ? m.result.exceptionDetails.exception.description : m.result.exceptionDetails.text);
    }
  });

  setTimeout(() => { ws.close(); process.exit(); }, 15000);
}
