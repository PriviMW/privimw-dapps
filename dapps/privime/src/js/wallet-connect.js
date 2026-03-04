'use strict';

import { setBEAM, initLock, setInitLock } from './state.js';
import { setSplashStatus } from './helpers.js';
import { handleApiResult } from './wallet-api.js';
import { onWalletConnected } from './startup.js';

// ================================================================
// WALLET CONNECTION
// ================================================================
export function initDesktopWallet() {
    var script = document.createElement('script');
    script.src = 'qrc:///qtwebchannel/qwebchannel.js';
    script.onload = function() {
        try {
            new QWebChannel(qt.webChannelTransport, function(channel) {
                setBEAM(channel.objects.BEAM);
                // Re-read BEAM after setting
                var beam = channel.objects.BEAM;
                if (!beam || !beam.api) { setSplashStatus('BEAM API not found'); return; }
                if (beam.api.callWalletApiResult && typeof beam.api.callWalletApiResult.connect === 'function') {
                    beam.api.callWalletApiResult.connect(handleApiResult);
                } else if (typeof beam.api.callWalletApiResult === 'function') {
                    beam.api.callWalletApiResult(handleApiResult);
                } else {
                    beam.api.callWalletApiResult = handleApiResult;
                }
                onWalletConnected();
            });
        } catch (e) { setSplashStatus('WebChannel error: ' + e.message); }
    };
    script.onerror = function() { setSplashStatus('Failed to load Qt WebChannel'); };
    document.head.appendChild(script);
}

export function detectAndConnect() {
    if (initLock) return;
    setInitLock(true);
    setSplashStatus('Connecting to Beam Wallet...');

    if (typeof qt !== 'undefined') {
        initDesktopWallet();
    } else if (window.BEAM) {
        setBEAM(window.BEAM);
        var beam = window.BEAM;
        if (/android/i.test(navigator.userAgent)) {
            document.addEventListener('onCallWalletApiResult', function(ev) { handleApiResult(ev.detail); });
        } else if (typeof beam.callWalletApiResult === 'function') {
            beam.callWalletApiResult(handleApiResult);
        }
        onWalletConnected();
    } else {
        setSplashStatus('No Beam Wallet detected. Open from within the Beam Desktop Wallet.');
    }
}
