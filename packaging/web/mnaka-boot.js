// Starts the Qt for WebAssembly client. Kept in its own file so the page needs no inline script.
(async () => {
    const shell = document.querySelector('#shell');
    const screen = document.querySelector('#screen');
    const status = document.querySelector('#status');
    const show = ui => { shell.hidden = ui !== shell; screen.hidden = ui !== screen; };
    if (typeof WebAssembly !== 'object') { status.textContent = 'This browser does not support WebAssembly.'; return; }
    // Optional deployment configuration. Without it the client has no account service.
    const environment = {};
    try {
        const response = await fetch('mnaka-config.json', { cache: 'no-store' });
        const config = response.ok ? await response.json() : {};
        if (typeof config.supabaseUrl === 'string' && typeof config.supabasePublishableKey === 'string') {
            environment.MNAKA_SUPABASE_URL = config.supabaseUrl;
            environment.MNAKA_SUPABASE_PUBLISHABLE_KEY = config.supabasePublishableKey;
        }
    } catch (error) {
        console.warn('mnaka-config.json was not usable; continuing without an account service.');
    }
    try {
        // Qt sizes its canvas from the container, so it has to be visible before load.
        screen.hidden = false;
        await qtLoad({
            qt: {
                onLoaded: () => show(screen),
                onExit: exit => {
                    status.textContent = 'MNAKA stopped' + (exit.code !== undefined ? ` (code ${exit.code})` : '') + '. Reload to restart.';
                    show(shell);
                },
                entryFunction: window.mnaka_client_entry,
                containerElements: [screen],
                environment
            }
        });
    } catch (error) {
        console.error(error);
        status.textContent = 'MNAKA could not start. Reload to try again.';
        show(shell);
    }
})();
