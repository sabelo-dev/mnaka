// Starts the Qt for WebAssembly client. Kept in its own file so the page needs no inline script.
(async () => {
    const shell = document.querySelector('#shell');
    const screen = document.querySelector('#screen');
    const status = document.querySelector('#status');
    const show = ui => { shell.hidden = ui !== shell; screen.hidden = ui !== screen; };
    if (typeof WebAssembly !== 'object') { status.textContent = 'This browser does not support WebAssembly.'; return; }
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
                containerElements: [screen]
            }
        });
    } catch (error) {
        console.error(error);
        status.textContent = 'MNAKA could not start. Reload to try again.';
        show(shell);
    }
})();
