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

    // Run comprehensive diagnostic
    send('Runtime.evaluate', {
      expression: `(async () => {
        const results = {};

        // Helper to call wallet API
        function callApi(method, params) {
          return new Promise((resolve) => {
            const callId = 'diag-' + method + '-' + Date.now();
            new QWebChannel(qt.webChannelTransport, (channel) => {
              const api = channel.objects.BEAM.api;
              const handler = (json) => {
                try {
                  const answer = JSON.parse(json);
                  if (answer.id === callId) {
                    resolve(answer);
                  }
                } catch(e) {}
              };
              api.callWalletApiResult.connect(handler);
              api.callWalletApi(JSON.stringify({
                jsonrpc: '2.0',
                id: callId,
                method: method,
                params: params || {}
              }));
            });
            setTimeout(() => resolve({error: 'TIMEOUT'}), 8000);
          });
        }

        // 1. Check wallet balance
        const walletStatus = await callApi('wallet_status');
        results.wallet = {
          available: walletStatus.result ? walletStatus.result.available : 'N/A',
          receiving: walletStatus.result ? walletStatus.result.receiving : 'N/A',
          sending: walletStatus.result ? walletStatus.result.sending : 'N/A',
          maturing: walletStatus.result ? walletStatus.result.maturing : 'N/A',
          current_height: walletStatus.result ? walletStatus.result.current_height : 'N/A'
        };

        // 2. Check pool info via shader (view_pool - read-only, no tx)
        const poolResult = await callApi('invoke_contract', {
          args: 'role=manager,action=view_pool,cid=e28c2b241a6871f4290d7e0c80b701273bbb48ee79ce5712f5873cdae3330c16',
          create_tx: false
        });
        if (poolResult.result && poolResult.result.output) {
          results.pool = JSON.parse(poolResult.result.output);
        } else {
          results.pool = poolResult.error || 'Failed';
        }

        // 3. Check contract params via shader
        const paramsResult = await callApi('invoke_contract', {
          args: 'role=user,action=view_params,cid=e28c2b241a6871f4290d7e0c80b701273bbb48ee79ce5712f5873cdae3330c16',
          create_tx: false
        });
        if (paramsResult.result && paramsResult.result.output) {
          results.params = JSON.parse(paramsResult.result.output);
        } else {
          results.params = paramsResult.error || 'Failed';
        }

        // 4. Try invoke_contract for place_bet with create_tx:true (single-step)
        // This bypasses process_invoke_data entirely
        const betResult = await callApi('invoke_contract', {
          args: 'role=user,action=place_bet,cid=e28c2b241a6871f4290d7e0c80b701273bbb48ee79ce5712f5873cdae3330c16,amount=100000000,asset_id=0,bet_type=0,exact_number=0,commitment=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
          create_tx: true
        });
        results.bet_create_tx_true = {
          has_result: !!betResult.result,
          has_error: !!betResult.error,
          error: betResult.error || null,
          txid: betResult.result ? betResult.result.txid : null,
          output: betResult.result ? betResult.result.output : null,
          raw_data_length: betResult.result && betResult.result.raw_data ? betResult.result.raw_data.length : 0
        };

        return JSON.stringify(results, null, 2);
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

  setTimeout(() => { ws.close(); process.exit(); }, 30000);
}
