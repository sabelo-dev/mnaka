// Starts the Qt for WebAssembly client. Kept in its own file so the page needs no inline script.
// It downloads the application with visible progress, retries on a flaky connection, waits when
// offline, and hands sign-in links (confirmation, expired link) to the application.
const bootScript = document.currentScript;
(async () => {
    const shell = document.querySelector('#shell');
    const screen = document.querySelector('#screen');
    const status = document.querySelector('#status');
    const progress = document.querySelector('#progress');
    const bar = document.querySelector('#progress-bar');
    const retry = document.querySelector('#retry');
    const release = bootScript.dataset.release;
    const expectedBytes = Number(bootScript.dataset.wasmBytes) || 0;
    const show = ui => { shell.hidden = ui !== shell; screen.hidden = ui !== screen; };
    const say = text => { status.textContent = text; };
    const megabytes = bytes => (bytes / 1048576).toFixed(1);
    const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
    const setProgress = fraction => {
        progress.hidden = fraction === null;
        if (fraction !== null) { bar.style.width = Math.round(fraction * 100) + '%'; progress.setAttribute('aria-valuenow', String(Math.round(fraction * 100))); }
    };
    // Ends in a state the user can act on: a message and a button that starts again.
    const giveUp = text => {
        setProgress(null); say(text); show(shell);
        retry.hidden = false; retry.focus();
    };
    retry.addEventListener('click', () => location.reload());
    if (typeof WebAssembly !== 'object') { say('This browser does not support WebAssembly, which MNAKA needs.'); return; }

    // A link from a sign-in email returns here with its result after the '#'. Take it out of the
    // address bar straight away so tokens are not left in history, and pass it to the application.
    const environment = {};
    const fragment = new URLSearchParams(location.hash.replace(/^#/, ''));
    if (fragment.has('error') || fragment.has('error_code')) {
        environment.MNAKA_AUTH_ERROR = (fragment.get('error_code') || fragment.get('error') || 'error').slice(0, 80);
        environment.MNAKA_AUTH_ERROR_DESCRIPTION = (fragment.get('error_description') || '').slice(0, 300);
    } else if (fragment.has('refresh_token')) {
        environment.MNAKA_AUTH_REFRESH_TOKEN = fragment.get('refresh_token').slice(0, 2000);
    }
    if (location.hash) history.replaceState(null, '', location.pathname + location.search);
    // Where confirmation emails should send people back to.
    environment.MNAKA_SITE_URL = location.origin + location.pathname;

    const fetchJson = async (path, timeoutMs) => {
        const response = await fetch(path, { cache: 'no-store', signal: AbortSignal.timeout(timeoutMs) });
        return response.ok ? await response.json() : null;
    };
    // This page is one release; if the server already has another, the page itself is stale.
    try {
        const current = await fetchJson('release.json', 8000);
        const reloaded = sessionStorage.getItem('mnaka-reloaded-for');
        if (current && typeof current.id === 'string' && current.id !== release && reloaded !== current.id) {
            sessionStorage.setItem('mnaka-reloaded-for', current.id);
            say('Updating to the latest version…');
            location.reload();
            return;
        }
    } catch (error) { /* offline or blocked: carry on with the release this page names */ }
    // Optional deployment configuration. Without it the client has no account service.
    try {
        const config = await fetchJson('mnaka-config.json', 8000) || {};
        if (typeof config.supabaseUrl === 'string' && typeof config.supabasePublishableKey === 'string') {
            environment.MNAKA_SUPABASE_URL = config.supabaseUrl;
            environment.MNAKA_SUPABASE_PUBLISHABLE_KEY = config.supabasePublishableKey;
        }
    } catch (error) {
        console.warn('mnaka-config.json was not usable; continuing without an account service.');
    }

    // One attempt at the application download, reporting progress and giving up if it stalls.
    const download = async url => {
        const controller = new AbortController();
        let stall = setTimeout(() => controller.abort(), 25000);
        try {
            const response = await fetch(url, { signal: controller.signal });
            if (!response.ok) throw new Error('HTTP ' + response.status);
            const total = Number(response.headers.get('Content-Length')) || expectedBytes;
            const reader = response.body.getReader();
            const chunks = []; let received = 0;
            for (;;) {
                const { done, value } = await reader.read();
                if (done) break;
                chunks.push(value); received += value.length;
                clearTimeout(stall); stall = setTimeout(() => controller.abort(), 25000);
                if (total) { setProgress(Math.min(1, received / total)); say(`Downloading MNAKA… ${megabytes(received)} of ${megabytes(total)} MB`); }
                else say(`Downloading MNAKA… ${megabytes(received)} MB`);
            }
            if (expectedBytes && received !== expectedBytes) throw new Error('incomplete download');
            const bytes = new Uint8Array(received); let offset = 0;
            for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
            return bytes;
        } finally { clearTimeout(stall); }
    };
    const waitUntilOnline = () => new Promise(resolve => {
        if (navigator.onLine) { resolve(); return; }
        say('You are offline. MNAKA will continue when the connection returns.');
        setProgress(null);
        addEventListener('online', resolve, { once: true });
    });
    const wasmUrl = new URL('mnaka_client.wasm', bootScript.src);
    let bytes = null;
    for (let attempt = 1; attempt <= 4 && !bytes; ++attempt) {
        await waitUntilOnline();
        try {
            setProgress(0); say('Downloading MNAKA…');
            bytes = await download(wasmUrl);
        } catch (error) {
            console.warn('application download failed', attempt, error && error.message);
            if (attempt < 4) { setProgress(null); say(`The download was interrupted. Trying again (${attempt + 1} of 4)…`); await sleep(1500 * attempt); }
        }
    }
    if (!bytes) { giveUp('MNAKA could not be downloaded. Check your connection, then try again.'); return; }

    try {
        setProgress(null); say('Starting MNAKA…');
        // Qt sizes its canvas from the container, so it has to be visible before load.
        screen.hidden = false;
        await qtLoad({
            qt: {
                module: WebAssembly.compile(bytes),
                onLoaded: () => show(screen),
                onExit: exit => giveUp(exit && exit.crashed ? 'MNAKA stopped unexpectedly.' : 'MNAKA has closed.'),
                entryFunction: window.mnaka_client_entry,
                containerElements: [screen],
                environment
            }
        });
    } catch (error) {
        console.error(error);
        giveUp('MNAKA could not start on this device.');
    }
})();
